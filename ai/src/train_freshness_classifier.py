"""Train a deployable fresh/risk classifier from position-level spectra."""

from __future__ import annotations

import argparse
import json
import os
from dataclasses import dataclass
from pathlib import Path

os.environ.setdefault(
    "MPLCONFIGDIR",
    str(Path(__file__).resolve().parents[1] / ".cache" / "matplotlib"),
)

import joblib
import matplotlib.pyplot as plt
import numpy as np
import pandas as pd
from sklearn.base import clone
from sklearn.ensemble import RandomForestClassifier
from sklearn.linear_model import LogisticRegression
from sklearn.metrics import (
    accuracy_score,
    balanced_accuracy_score,
    confusion_matrix,
    f1_score,
)
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.svm import SVC


PROJECT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = (
    PROJECT_ROOT
    / "data"
    / "processed"
    / "NECT_FRESHNESS_position_summary.csv"
)
DEFAULT_OVERRIDES = (
    PROJECT_ROOT / "config" / "freshness_training_overrides.csv"
)
MODELS_DIR = PROJECT_ROOT / "models"
FIGURES_DIR = PROJECT_ROOT / "figures"

FEATURES = [
    "F1_415nm_clear_norm",
    "F2_445nm_clear_norm",
    "F3_480nm_clear_norm",
    "F4_515nm_clear_norm",
    "F5_555nm_clear_norm",
    "F6_590nm_clear_norm",
    "F7_630nm_clear_norm",
    "F8_680nm_clear_norm",
    "NIR_clear_norm",
]
LABEL_MAP = {
    "fresh": "fresh",
    "warning": "risk",
    "spoiled": "risk",
}
LOCAL_FOREST_TREES = 8
LOCAL_FOREST_MAX_DEPTH = 3
LOCAL_FOREST_MIN_SAMPLES_LEAF = 2
LOCAL_FOREST_RISK_THRESHOLD = 0.55


@dataclass
class CvResult:
    name: str
    accuracy: float
    balanced_accuracy: float
    risk_f1: float
    predictions: np.ndarray
    risk_probabilities: np.ndarray
    folds: list[dict[str, object]]
    daily_accuracy: float
    daily_balanced_accuracy: float
    daily_risk_f1: float
    daily_predictions: pd.DataFrame


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Train a fresh/risk model with leave-one-fruit-out validation "
            "and export ESP32 logistic plus local-forest parameters."
        )
    )
    parser.add_argument("--input", default=str(DEFAULT_INPUT))
    parser.add_argument("--training-overrides", default=str(DEFAULT_OVERRIDES))
    parser.add_argument("--output-prefix", default="NECT_FRESHNESS_V1")
    parser.add_argument(
        "--esp-header",
        help="Optional additional ESP-IDF header output path",
    )
    return parser.parse_args()


def resolve_path(value: str) -> Path:
    path = Path(value)
    return path if path.is_absolute() else PROJECT_ROOT / path


def apply_training_overrides(
    data: pd.DataFrame,
    overrides_path: Path,
) -> tuple[pd.DataFrame, pd.DataFrame]:
    result = data.copy()
    result["training_included"] = 1
    result["training_label"] = result["freshness_state"].map(LABEL_MAP)
    result["training_override_reason"] = ""
    audit_rows: list[dict[str, object]] = []

    if not overrides_path.exists():
        return result, pd.DataFrame(audit_rows)

    overrides = pd.read_csv(overrides_path, dtype=str).fillna("")
    required = {
        "sample_id",
        "storage_day",
        "position",
        "action",
        "training_label",
        "reason",
    }
    missing = sorted(required.difference(overrides.columns))
    if missing:
        raise ValueError(
            f"Training override CSV is missing columns: {', '.join(missing)}"
        )

    for override in overrides.itertuples(index=False):
        mask = (
            result["sample_id"].astype(str).eq(str(override.sample_id).strip())
            & result["storage_day"]
            .astype(str)
            .eq(str(override.storage_day).strip())
            & result["position"].astype(str).eq(str(override.position).strip())
        )
        if int(mask.sum()) != 1:
            raise ValueError(
                "Training override must match exactly one position row: "
                f"{override.sample_id}/day{override.storage_day}/pos{override.position}"
            )

        action = str(override.action).strip().lower()
        if action not in {"include", "exclude"}:
            raise ValueError(f"Unsupported training override action: {action}")
        result.loc[mask, "training_included"] = int(action == "include")

        label = str(override.training_label).strip()
        if label:
            if label not in {"fresh", "risk"}:
                raise ValueError(f"Unsupported training label: {label}")
            result.loc[mask, "training_label"] = label

        reason = str(override.reason).strip()
        result.loc[mask, "training_override_reason"] = reason
        audit_rows.append(
            {
                "sample_id": str(override.sample_id).strip(),
                "storage_day": int(override.storage_day),
                "position": str(override.position).strip(),
                "action": action,
                "training_label": label,
                "reason": reason,
            }
        )

    return result, pd.DataFrame(audit_rows)


def prepare_data(
    input_path: Path,
    overrides_path: Path,
) -> tuple[pd.DataFrame, pd.DataFrame, pd.DataFrame]:
    data = pd.read_csv(input_path)
    required = {
        "sample_id",
        "storage_day",
        "freshness_state",
        "position",
        "valid_frames",
        *FEATURES,
    }
    missing = sorted(required.difference(data.columns))
    if missing:
        raise ValueError(f"Position summary is missing columns: {', '.join(missing)}")

    data["sample_id"] = data["sample_id"].astype(str)
    data["position"] = data["position"].astype(str)
    data["storage_day"] = pd.to_numeric(
        data["storage_day"], errors="raise"
    ).astype(int)
    data["valid_frames"] = pd.to_numeric(
        data["valid_frames"], errors="coerce"
    )
    for feature in FEATURES:
        data[feature] = pd.to_numeric(data[feature], errors="coerce")

    data, audit = apply_training_overrides(data, overrides_path)
    included = data.loc[data["training_included"] == 1].copy()
    included = included.loc[included["valid_frames"] >= 20].copy()
    if included[FEATURES].isna().any().any():
        raise ValueError("Included training spectra contain missing features")
    if included["training_label"].isna().any():
        raise ValueError("Included rows contain an unmapped freshness_state")

    labels = set(included["training_label"].astype(str))
    if labels != {"fresh", "risk"}:
        raise ValueError(f"Training requires fresh and risk labels, got: {labels}")

    fruit_counts = included.groupby("training_label")["sample_id"].nunique()
    if (fruit_counts < 2).any():
        details = ", ".join(
            f"{label}={int(count)} fruit(s)"
            for label, count in fruit_counts.items()
        )
        raise ValueError(
            "Each binary class needs at least two independent fruits: " + details
        )

    return included.reset_index(drop=True), data, audit


def build_models() -> dict[str, object]:
    return {
        "logistic": Pipeline(
            [
                ("scaler", StandardScaler()),
                (
                    "classifier",
                    LogisticRegression(
                        C=1.0,
                        class_weight="balanced",
                        max_iter=5000,
                        random_state=42,
                    ),
                ),
            ]
        ),
        "random_forest": RandomForestClassifier(
            n_estimators=LOCAL_FOREST_TREES,
            max_depth=LOCAL_FOREST_MAX_DEPTH,
            min_samples_leaf=LOCAL_FOREST_MIN_SAMPLES_LEAF,
            class_weight="balanced",
            random_state=42,
            n_jobs=-1,
        ),
        "svm_rbf": Pipeline(
            [
                ("scaler", StandardScaler()),
                (
                    "classifier",
                    SVC(
                        C=1.0,
                        gamma="scale",
                        class_weight="balanced",
                        random_state=42,
                    ),
                ),
            ]
        ),
    }


def risk_probability(model: object, values: pd.DataFrame) -> np.ndarray:
    classes = [str(value) for value in model.classes_]
    if "risk" not in classes:
        raise ValueError("Model has no risk class")
    risk_index = classes.index("risk")
    if hasattr(model, "predict_proba"):
        return np.asarray(
            model.predict_proba(values)[:, risk_index], dtype=float
        )

    decision = np.asarray(model.decision_function(values), dtype=float)
    if len(classes) != 2:
        raise ValueError("Decision-function probability only supports binary models")
    if risk_index == 0:
        decision = -decision
    return 1.0 / (1.0 + np.exp(-decision))


def aggregate_position_predictions(
    data: pd.DataFrame,
    predictions: np.ndarray,
    probabilities: np.ndarray,
    threshold: float = 0.5,
) -> pd.DataFrame:
    rows = data[
        [
            "sample_id",
            "storage_day",
            "freshness_state",
            "training_label",
        ]
    ].copy()
    rows["position_prediction"] = predictions
    rows["risk_probability"] = probabilities
    grouped_rows: list[dict[str, object]] = []
    for keys, group in rows.groupby(
        ["sample_id", "storage_day", "training_label"], sort=True
    ):
        sample_id, storage_day, training_label = keys
        probability = float(group["risk_probability"].mean())
        grouped_rows.append(
            {
                "sample_id": str(sample_id),
                "storage_day": int(storage_day),
                "training_label": str(training_label),
                "positions": int(len(group)),
                "mean_risk_probability": probability,
                "predicted_label": (
                    "risk" if probability >= threshold else "fresh"
                ),
            }
        )
    result = pd.DataFrame(grouped_rows)
    result["correct"] = (
        result["training_label"] == result["predicted_label"]
    ).astype(int)
    return result


def leave_one_fruit_out(
    data: pd.DataFrame,
    model: object,
    name: str,
) -> CvResult:
    predictions = np.empty(len(data), dtype=object)
    probabilities = np.full(len(data), np.nan, dtype=float)
    folds: list[dict[str, object]] = []

    for held_fruit in sorted(data["sample_id"].unique()):
        test_mask = data["sample_id"].eq(held_fruit)
        train_mask = ~test_mask
        train_labels = set(data.loc[train_mask, "training_label"])
        if train_labels != {"fresh", "risk"}:
            raise ValueError(
                f"Leaving out {held_fruit} removes a binary training class"
            )

        fold_model = clone(model)
        fold_model.fit(
            data.loc[train_mask, FEATURES],
            data.loc[train_mask, "training_label"],
        )
        fold_predictions = fold_model.predict(data.loc[test_mask, FEATURES])
        fold_probabilities = risk_probability(
            fold_model, data.loc[test_mask, FEATURES]
        )
        predictions[test_mask.to_numpy()] = fold_predictions
        probabilities[test_mask.to_numpy()] = fold_probabilities
        true_labels = data.loc[test_mask, "training_label"]
        folds.append(
            {
                "model": name,
                "held_fruit": held_fruit,
                "test_positions": int(test_mask.sum()),
                "true_states": ",".join(sorted(set(true_labels))),
                "accuracy": float(
                    accuracy_score(true_labels, fold_predictions)
                ),
            }
        )

    true = data["training_label"].astype(str)
    daily_predictions = aggregate_position_predictions(
        data, predictions, probabilities
    )
    daily_true = daily_predictions["training_label"]
    daily_predicted = daily_predictions["predicted_label"]
    return CvResult(
        name=name,
        accuracy=float(accuracy_score(true, predictions)),
        balanced_accuracy=float(balanced_accuracy_score(true, predictions)),
        risk_f1=float(
            f1_score(true, predictions, pos_label="risk", zero_division=0)
        ),
        predictions=predictions,
        risk_probabilities=probabilities,
        folds=folds,
        daily_accuracy=float(
            accuracy_score(daily_true, daily_predicted)
        ),
        daily_balanced_accuracy=float(
            balanced_accuracy_score(daily_true, daily_predicted)
        ),
        daily_risk_f1=float(
            f1_score(
                daily_true,
                daily_predicted,
                pos_label="risk",
                zero_division=0,
            )
        ),
        daily_predictions=daily_predictions,
    )


def c_float(value: float) -> str:
    text = f"{value:.9g}"
    if "." not in text and "e" not in text.lower():
        text += ".0"
    return f"{text}f"


def c_array(values: np.ndarray) -> str:
    return ",\n".join(
        "    " + ", ".join(c_float(value) for value in values[i : i + 5])
        for i in range(0, len(values), 5)
    )


def serialize_local_forest(
    model: RandomForestClassifier,
) -> tuple[np.ndarray, list[tuple[int, int, int, float, float]]]:
    classes = [str(value) for value in model.classes_]
    if classes != ["fresh", "risk"]:
        raise ValueError(f"Unexpected local forest class order: {classes}")
    risk_index = classes.index("risk")
    roots: list[int] = []
    nodes: list[tuple[int, int, int, float, float]] = []

    for estimator in model.estimators_:
        tree = estimator.tree_
        offset = len(nodes)
        roots.append(offset)
        for node_index in range(tree.node_count):
            feature = int(tree.feature[node_index])
            is_leaf = feature < 0
            left = -1 if is_leaf else offset + int(tree.children_left[node_index])
            right = -1 if is_leaf else offset + int(tree.children_right[node_index])
            threshold = 0.0 if is_leaf else float(tree.threshold[node_index])
            values = np.asarray(tree.value[node_index][0], dtype=float)
            total = float(values.sum())
            risk = 0.0 if total <= 0.0 else float(values[risk_index] / total)
            nodes.append((feature, left, right, threshold, risk))

    if len(nodes) > np.iinfo(np.int16).max:
        raise ValueError("Local forest has too many nodes for int16 indexes")
    return np.asarray(roots, dtype=np.uint16), nodes


def c_local_forest(
    model: RandomForestClassifier,
) -> tuple[str, str]:
    roots, nodes = serialize_local_forest(model)
    root_text = ", ".join(str(int(value)) for value in roots)
    node_lines = [
        "    {"
        f"{feature}, {left}, {right}, "
        f"{c_float(threshold)}, {c_float(risk)}"
        "}"
        for feature, left, right, threshold, risk in nodes
    ]
    return root_text, ",\n".join(node_lines)


def export_logistic_header(
    model: Pipeline,
    local_model: RandomForestClassifier,
    output_path: Path,
    source_dataset: Path,
) -> None:
    scaler: StandardScaler = model.named_steps["scaler"]
    classifier: LogisticRegression = model.named_steps["classifier"]
    classes = [str(value) for value in classifier.classes_]
    if classes != ["fresh", "risk"]:
        raise ValueError(f"Unexpected binary class order: {classes}")
    weights = np.asarray(classifier.coef_, dtype=float)
    if weights.shape != (1, len(FEATURES)):
        raise ValueError(f"Unexpected binary logistic shape: {weights.shape}")

    feature_names = ",\n".join(f'    "{name}"' for name in FEATURES)
    local_roots, local_nodes = c_local_forest(local_model)
    local_node_count = sum(
        estimator.tree_.node_count for estimator in local_model.estimators_
    )
    content = f"""/* Auto-generated by train_freshness_classifier.py.
 * Dataset: {source_dataset.name}
 * Positive sigmoid output: risk
 */
#ifndef NECT_FRESHNESS_MODEL_H
#define NECT_FRESHNESS_MODEL_H

#include <stdint.h>

#define FRESHNESS_MODEL_FEATURE_COUNT {len(FEATURES)}
#define FRESHNESS_MODEL_DEFAULT_THRESHOLD 0.5f
#define FRESHNESS_LOCAL_FOREST_TREE_COUNT {len(local_model.estimators_)}U
#define FRESHNESS_LOCAL_FOREST_NODE_COUNT {local_node_count}U
#define FRESHNESS_LOCAL_FOREST_RISK_THRESHOLD {c_float(LOCAL_FOREST_RISK_THRESHOLD)}

typedef struct {{
    int8_t feature;
    int16_t left;
    int16_t right;
    float threshold;
    float risk_probability;
}} freshness_local_forest_node_t;

static const char *const FRESHNESS_MODEL_FEATURES[FRESHNESS_MODEL_FEATURE_COUNT] = {{
{feature_names}
}};

static const float FRESHNESS_SCALER_MEAN[FRESHNESS_MODEL_FEATURE_COUNT] = {{
{c_array(np.asarray(scaler.mean_, dtype=float))}
}};

static const float FRESHNESS_SCALER_SCALE[FRESHNESS_MODEL_FEATURE_COUNT] = {{
{c_array(np.asarray(scaler.scale_, dtype=float))}
}};

static const float FRESHNESS_RISK_WEIGHTS[FRESHNESS_MODEL_FEATURE_COUNT] = {{
{c_array(weights[0])}
}};

static const float FRESHNESS_RISK_BIAS = {c_float(float(classifier.intercept_[0]))};

static const uint16_t
FRESHNESS_LOCAL_FOREST_ROOTS[FRESHNESS_LOCAL_FOREST_TREE_COUNT] = {{
    {local_roots}
}};

static const freshness_local_forest_node_t
FRESHNESS_LOCAL_FOREST_NODES[FRESHNESS_LOCAL_FOREST_NODE_COUNT] = {{
{local_nodes}
}};

#endif /* NECT_FRESHNESS_MODEL_H */
"""
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(content, encoding="utf-8", newline="\n")


def verify_logistic_export(model: Pipeline, data: pd.DataFrame) -> None:
    scaler: StandardScaler = model.named_steps["scaler"]
    classifier: LogisticRegression = model.named_steps["classifier"]
    values = data[FEATURES].to_numpy(dtype=np.float32)
    standardized = (
        values - np.asarray(scaler.mean_, dtype=np.float32)
    ) / np.asarray(scaler.scale_, dtype=np.float32)
    logits = (
        standardized @ np.asarray(classifier.coef_[0], dtype=np.float32)
        + np.float32(classifier.intercept_[0])
    )
    manual = 1.0 / (1.0 + np.exp(-logits))
    python = risk_probability(model, data[FEATURES])
    if not np.allclose(manual, python, rtol=1e-5, atol=1e-6):
        raise RuntimeError("Float32 exported risk probabilities differ from Python")


def verify_local_forest_export(
    model: RandomForestClassifier,
    data: pd.DataFrame,
) -> None:
    roots, nodes = serialize_local_forest(model)
    values = data[FEATURES].to_numpy(dtype=np.float32)
    manual = np.zeros(len(values), dtype=np.float32)
    for row_index, row in enumerate(values):
        probability_sum = np.float32(0.0)
        for root in roots:
            node_index = int(root)
            while nodes[node_index][0] >= 0:
                feature, left, right, threshold, _ = nodes[node_index]
                node_index = left if row[feature] <= np.float32(threshold) else right
            probability_sum += np.float32(nodes[node_index][4])
        manual[row_index] = probability_sum / np.float32(len(roots))

    python = risk_probability(model, data[FEATURES])
    if not np.allclose(manual, python, rtol=1e-5, atol=1e-6):
        raise RuntimeError("Float32 exported local forest differs from Python")


def save_figure(
    data: pd.DataFrame,
    results: list[CvResult],
    selected: CvResult,
    path: Path,
) -> None:
    labels = ["fresh", "risk"]
    matrix = confusion_matrix(
        selected.daily_predictions["training_label"],
        selected.daily_predictions["predicted_label"],
        labels=labels,
    )
    figure, axes = plt.subplots(1, 3, figsize=(17, 5))

    image = axes[0].imshow(matrix, cmap="Blues")
    axes[0].set_title("Four-position fruit-day confusion matrix")
    axes[0].set_xticks(range(2), labels)
    axes[0].set_yticks(range(2), labels)
    axes[0].set_xlabel("Predicted")
    axes[0].set_ylabel("True")
    for row in range(2):
        for column in range(2):
            axes[0].text(
                column,
                row,
                str(matrix[row, column]),
                ha="center",
                va="center",
            )
    figure.colorbar(image, ax=axes[0], fraction=0.046)

    x = np.arange(len(results))
    width = 0.36
    axes[1].bar(
        x - width / 2,
        [item.daily_balanced_accuracy * 100 for item in results],
        width,
        label="Balanced accuracy",
    )
    axes[1].bar(
        x + width / 2,
        [item.daily_risk_f1 * 100 for item in results],
        width,
        label="Risk F1",
    )
    axes[1].set_xticks(x, [item.name for item in results], rotation=25)
    axes[1].set_ylim(0, 105)
    axes[1].set_ylabel("Score (%)")
    axes[1].set_title("Model comparison")
    axes[1].legend()

    plot_data = selected.daily_predictions.copy()
    plot_data["risk_probability"] = plot_data["mean_risk_probability"]
    colors = plot_data["training_label"].map(
        {"fresh": "#2f9e44", "risk": "#d9480f"}
    )
    axes[2].scatter(
        np.arange(len(plot_data)),
        plot_data["risk_probability"],
        c=colors,
        alpha=0.85,
    )
    axes[2].axhline(0.5, color="black", linestyle="--", linewidth=1)
    axes[2].set_ylim(-0.05, 1.05)
    axes[2].set_xlabel("Held-out fruit-day sample")
    axes[2].set_ylabel("Risk probability")
    axes[2].set_title("Four-position mean risk probabilities")

    figure.suptitle("NECTARINE fresh/risk model V1")
    figure.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180)
    plt.close(figure)


def run(args: argparse.Namespace) -> int:
    input_path = resolve_path(args.input)
    overrides_path = resolve_path(args.training_overrides)
    if not input_path.exists():
        raise FileNotFoundError(input_path)

    data, annotated, override_audit = prepare_data(
        input_path, overrides_path
    )
    models = build_models()
    results = [
        leave_one_fruit_out(data, model, name)
        for name, model in models.items()
    ]
    selected = next(item for item in results if item.name == "logistic")

    deployment_model: Pipeline = clone(models["logistic"])
    deployment_model.fit(data[FEATURES], data["training_label"])
    verify_logistic_export(deployment_model, data)
    local_model: RandomForestClassifier = clone(models["random_forest"])
    local_model.fit(data[FEATURES], data["training_label"])
    verify_local_forest_export(local_model, data)

    prefix = args.output_prefix.replace("/", "_").replace("\\", "_")
    MODELS_DIR.mkdir(parents=True, exist_ok=True)
    FIGURES_DIR.mkdir(parents=True, exist_ok=True)
    model_path = MODELS_DIR / f"{prefix}_logistic.joblib"
    local_model_path = MODELS_DIR / f"{prefix}_local_forest.joblib"
    header_path = MODELS_DIR / f"{prefix}_freshness_model.h"
    metrics_path = MODELS_DIR / f"{prefix}_cv_metrics.csv"
    folds_path = MODELS_DIR / f"{prefix}_cv_folds.csv"
    predictions_path = MODELS_DIR / f"{prefix}_cv_predictions.csv"
    daily_predictions_path = (
        MODELS_DIR / f"{prefix}_cv_daily_predictions.csv"
    )
    metadata_path = MODELS_DIR / f"{prefix}_model_metadata.json"
    audit_path = MODELS_DIR / f"{prefix}_training_overrides_applied.csv"
    annotated_path = MODELS_DIR / f"{prefix}_annotated_samples.csv"
    figure_path = FIGURES_DIR / f"{prefix}_model_evaluation.png"

    joblib.dump(deployment_model, model_path)
    joblib.dump(local_model, local_model_path)
    export_logistic_header(
        deployment_model, local_model, header_path, input_path
    )

    if args.esp_header:
        esp_header_path = resolve_path(args.esp_header)
        export_logistic_header(
            deployment_model, local_model, esp_header_path, input_path
        )
    else:
        esp_header_path = None

    metric_rows = [
        {
            "model": item.name,
            "validation": "leave_one_fruit_out",
            "accuracy": item.accuracy,
            "balanced_accuracy": item.balanced_accuracy,
            "risk_f1": item.risk_f1,
            "daily_accuracy": item.daily_accuracy,
            "daily_balanced_accuracy": item.daily_balanced_accuracy,
            "daily_risk_f1": item.daily_risk_f1,
            "deployment_selected": int(item.name == "logistic"),
            "local_auxiliary_selected": int(item.name == "random_forest"),
        }
        for item in results
    ]
    pd.DataFrame(metric_rows).to_csv(
        metrics_path, index=False, encoding="utf-8-sig"
    )
    pd.DataFrame(
        [row for item in results for row in item.folds]
    ).to_csv(folds_path, index=False, encoding="utf-8-sig")

    predictions = data[
        [
            "sample_id",
            "storage_day",
            "position",
            "freshness_state",
            "training_label",
        ]
    ].copy()
    predictions["predicted_label"] = selected.predictions
    predictions["risk_probability"] = selected.risk_probabilities
    predictions["correct"] = (
        predictions["training_label"] == predictions["predicted_label"]
    ).astype(int)
    predictions.to_csv(
        predictions_path, index=False, encoding="utf-8-sig"
    )
    selected.daily_predictions.to_csv(
        daily_predictions_path, index=False, encoding="utf-8-sig"
    )
    override_audit.to_csv(audit_path, index=False, encoding="utf-8-sig")
    annotated.to_csv(annotated_path, index=False, encoding="utf-8-sig")

    metadata = {
        "dataset": str(input_path),
        "target": "fresh_vs_risk",
        "risk_definition": "warning_or_spoiled",
        "sample_unit": "one fruit position after 20-frame aggregation",
        "decision_protocol": (
            "Average risk probabilities from four fruit positions; "
            "NECT06 localized-pit validation uses the confirmed pit position"
        ),
        "validation": "leave_one_fruit_out",
        "rows": int(len(data)),
        "fruit_ids": sorted(data["sample_id"].unique()),
        "fruit_count": int(data["sample_id"].nunique()),
        "class_rows": {
            str(key): int(value)
            for key, value in data["training_label"].value_counts().items()
        },
        "class_fruits": {
            str(key): int(value)
            for key, value in data.groupby("training_label")[
                "sample_id"
            ].nunique().items()
        },
        "features": FEATURES,
        "deployment_model": (
            "balanced_logistic_regression_with_8_tree_local_forest"
        ),
        "threshold": 0.5,
        "local_forest_threshold": LOCAL_FOREST_RISK_THRESHOLD,
        "local_forest_trees": LOCAL_FOREST_TREES,
        "local_forest_max_depth": LOCAL_FOREST_MAX_DEPTH,
        "held_out_accuracy": selected.accuracy,
        "held_out_balanced_accuracy": selected.balanced_accuracy,
        "held_out_risk_f1": selected.risk_f1,
        "held_out_daily_accuracy": selected.daily_accuracy,
        "held_out_daily_balanced_accuracy": selected.daily_balanced_accuracy,
        "held_out_daily_risk_f1": selected.daily_risk_f1,
        "all_results": metric_rows,
        "c_export_verified": True,
    }
    metadata_path.write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    save_figure(data, results, selected, figure_path)

    print("=== Nectarine fresh/risk model V1 ===")
    print(f"Input: {input_path}")
    print(
        f"Training rows: {len(data)} positions from "
        f"{data['sample_id'].nunique()} fruits"
    )
    print(
        "Class rows: "
        + ", ".join(
            f"{key}={value}"
            for key, value in data["training_label"].value_counts().items()
        )
    )
    print("\nLeave-one-fruit-out validation:")
    for item in results:
        print(
            f"  {item.name:<14} position_balanced="
            f"{item.balanced_accuracy * 100:6.2f}%  "
            f"fruit_day_accuracy={item.daily_accuracy * 100:6.2f}%  "
            f"fruit_day_balanced={item.daily_balanced_accuracy * 100:6.2f}%  "
            f"fruit_day_risk_f1={item.daily_risk_f1 * 100:6.2f}%"
        )
    print(
        "\nDeployment model: balanced logistic regression + "
        f"{LOCAL_FOREST_TREES}-tree local forest"
    )
    print(f"Model: {model_path}")
    print(f"Local model: {local_model_path}")
    print(f"ESP32 header: {header_path}")
    if esp_header_path is not None:
        print(f"Additional ESP32 header: {esp_header_path}")
    print(f"Predictions: {predictions_path}")
    print(f"Fruit-day predictions: {daily_predictions_path}")
    print(f"Metadata: {metadata_path}")
    print(f"Figure: {figure_path}")
    print("C export verification: logistic and local forest passed")
    return 0


def main() -> int:
    return run(parse_args())


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError, pd.errors.ParserError) as error:
        print(f"[ERROR] {error}")
        raise SystemExit(1)
