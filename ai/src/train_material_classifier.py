"""训练材料分类器，按测量位置验证，并导出ESP32可用的逻辑回归参数。"""

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
    classification_report,
    confusion_matrix,
)
from sklearn.pipeline import Pipeline
from sklearn.preprocessing import StandardScaler
from sklearn.svm import SVC


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PROCESSED_DIR = PROJECT_ROOT / "data" / "processed"
MODELS_DIR = PROJECT_ROOT / "models"
FIGURES_DIR = PROJECT_ROOT / "figures"

RAW_FEATURES = [
    "F1_415nm",
    "F2_445nm",
    "F3_480nm",
    "F4_515nm",
    "F5_555nm",
    "F6_590nm",
    "F7_630nm",
    "F8_680nm",
    "Clear",
    "NIR",
]
NORMALIZED_FEATURES = [
    f"{feature}_norm" for feature in RAW_FEATURES if feature != "Clear"
]


@dataclass
class CvResult:
    name: str
    model_name: str
    feature_set: str
    features: list[str]
    accuracy: float
    predictions: np.ndarray
    fold_rows: list[dict[str, object]]
    validation_group: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="按完整测量位置交叉验证材料分类模型并导出ESP32参数。"
    )
    parser.add_argument(
        "--input",
        help="合并后的pilot CSV；省略时自动选择最新的 *_pilot_combined.csv",
    )
    parser.add_argument(
        "--dataset-name",
        help="输出文件前缀；省略时从输入文件名推断，例如P2",
    )
    parser.add_argument(
        "--group-column",
        choices=["auto", "sample_id", "position"],
        default="auto",
        help="交叉验证分组；auto会在每类至少2个独立样品时使用sample_id，否则使用position",
    )
    parser.add_argument(
        "--esp-header",
        help="可选：额外把部署模型头文件写入ESP-IDF工程，例如 ../ESP-project/main/fruit_material_model.h",
    )
    return parser.parse_args()


def resolve_input_path(value: str | None) -> Path:
    if value:
        path = Path(value)
        return path if path.is_absolute() else PROJECT_ROOT / path

    candidates = sorted(
        PROCESSED_DIR.glob("*_pilot_combined.csv"),
        key=lambda path: path.stat().st_mtime,
        reverse=True,
    )
    if not candidates:
        raise FileNotFoundError("data/processed 中没有 *_pilot_combined.csv")
    return candidates[0]


def infer_dataset_name(path: Path) -> str:
    suffix = "_pilot_combined"
    return path.stem[: -len(suffix)] if path.stem.endswith(suffix) else path.stem


def prepare_data(path: Path) -> pd.DataFrame:
    data = pd.read_csv(path)
    required = {"label", "sample_id", "position", *RAW_FEATURES}
    missing = required.difference(data.columns)
    if missing:
        raise ValueError(f"训练数据缺少字段: {', '.join(sorted(missing))}")

    if "quality_valid" in data.columns:
        data = data.loc[data["quality_valid"] == 1].copy()
    if "distance_in_range" in data.columns:
        data = data.loc[data["distance_in_range"] == 1].copy()
    if data.empty:
        raise ValueError("质量筛选后没有训练数据")

    if data[RAW_FEATURES].isna().any().any():
        raise ValueError("原始光谱特征存在缺失值")
    if (data["Clear"] <= 0).any():
        raise ValueError("Clear必须大于0才能计算归一化特征")

    for feature in RAW_FEATURES:
        if feature == "Clear":
            continue
        data[f"{feature}_norm"] = data[feature] / data["Clear"]

    label_count = data["label"].nunique()
    if label_count < 2:
        raise ValueError("至少需要两个材料类别")

    return data.reset_index(drop=True)


def choose_validation_group(data: pd.DataFrame, requested: str) -> str:
    if requested != "auto":
        return requested

    specimens_per_label = data.groupby("label")["sample_id"].nunique()
    if (specimens_per_label >= 2).all():
        return "sample_id"
    return "position"


def build_models() -> dict[str, object]:
    return {
        "logistic": Pipeline(
            [
                ("scaler", StandardScaler()),
                (
                    "classifier",
                    LogisticRegression(max_iter=5000, random_state=42),
                ),
            ]
        ),
        "random_forest": RandomForestClassifier(
            n_estimators=300,
            max_depth=5,
            random_state=42,
            n_jobs=-1,
        ),
        "svm_rbf": Pipeline(
            [
                ("scaler", StandardScaler()),
                (
                    "classifier",
                    SVC(
                        kernel="rbf",
                        C=10.0,
                        gamma="scale",
                        decision_function_shape="ovr",
                        break_ties=True,
                        random_state=42,
                    ),
                ),
            ]
        ),
    }


def cross_validate_by_group(
    data: pd.DataFrame,
    model: object,
    model_name: str,
    feature_set: str,
    features: list[str],
    group_column: str,
) -> CvResult:
    predictions = np.empty(len(data), dtype=object)
    fold_rows: list[dict[str, object]] = []
    all_labels = set(data["label"].astype(str))

    groups = sorted(data[group_column].unique(), key=lambda value: str(value))
    if len(groups) < 2:
        raise ValueError(f"{group_column} 至少需要两个不同取值才能进行分组验证")

    for held_group in groups:
        test_mask = data[group_column] == held_group
        train_mask = ~test_mask

        train_labels = set(data.loc[train_mask, "label"].astype(str))
        if train_labels != all_labels:
            raise ValueError(
                f"留出 {group_column}={held_group} 后训练集缺少类别；"
                "按独立样品验证时每类至少需要2个水果"
            )

        fold_model = clone(model)
        fold_model.fit(
            data.loc[train_mask, features],
            data.loc[train_mask, "label"],
        )
        fold_predictions = fold_model.predict(data.loc[test_mask, features])
        predictions[test_mask.to_numpy()] = fold_predictions

        fold_rows.append(
            {
                "model": model_name,
                "feature_set": feature_set,
                "group_column": group_column,
                "held_group": str(held_group),
                "test_frames": int(test_mask.sum()),
                "accuracy": float(
                    accuracy_score(data.loc[test_mask, "label"], fold_predictions)
                ),
            }
        )

    name = f"{model_name}_{feature_set}"
    return CvResult(
        name=name,
        model_name=model_name,
        feature_set=feature_set,
        features=features,
        accuracy=float(accuracy_score(data["label"], predictions)),
        predictions=predictions,
        fold_rows=fold_rows,
        validation_group=group_column,
    )


def float_array(values: np.ndarray, indent: str = "    ") -> str:
    flat_values = np.asarray(values).reshape(-1)

    def c_float(value: float) -> str:
        text = f"{value:.9g}"
        if "." not in text and "e" not in text.lower():
            text += ".0"
        return f"{text}f"

    return ",\n".join(
        indent + ", ".join(c_float(value) for value in flat_values[i : i + 5])
        for i in range(0, len(flat_values), 5)
    )


def c_matrix(values: np.ndarray) -> str:
    return ",\n".join(
        "    {\n" + float_array(row, "        ") + "\n    }"
        for row in np.asarray(values)
    )


def export_logistic_c_header(
    model: Pipeline,
    features: list[str],
    path: Path,
    dataset_name: str,
) -> None:
    scaler: StandardScaler = model.named_steps["scaler"]
    classifier: LogisticRegression = model.named_steps["classifier"]
    classes = [str(value) for value in classifier.classes_]
    coefficients = np.asarray(classifier.coef_, dtype=float)
    intercept = np.asarray(classifier.intercept_, dtype=float)

    if coefficients.shape != (len(classes), len(features)):
        raise ValueError("只支持多类别逻辑回归的二维权重矩阵")

    guard = f"{dataset_name.upper()}_MATERIAL_MODEL_H".replace("-", "_")
    class_lines = ",\n".join(f'    "{name}"' for name in classes)
    feature_lines = ",\n".join(f'    "{name}"' for name in features)

    content = f"""/* Auto-generated by train_material_classifier.py. */
#ifndef {guard}
#define {guard}

#define MATERIAL_MODEL_IS_LOGISTIC 1
#define MATERIAL_MODEL_CLASS_COUNT {len(classes)}
#define MATERIAL_MODEL_FEATURE_COUNT {len(features)}

static const char *const MATERIAL_MODEL_CLASSES[MATERIAL_MODEL_CLASS_COUNT] = {{
{class_lines}
}};

static const char *const MATERIAL_MODEL_FEATURES[MATERIAL_MODEL_FEATURE_COUNT] = {{
{feature_lines}
}};

static const float MATERIAL_SCALER_MEAN[MATERIAL_MODEL_FEATURE_COUNT] = {{
{float_array(scaler.mean_)}
}};

static const float MATERIAL_SCALER_SCALE[MATERIAL_MODEL_FEATURE_COUNT] = {{
{float_array(scaler.scale_)}
}};

static const float MATERIAL_MODEL_WEIGHTS[MATERIAL_MODEL_CLASS_COUNT]
                                         [MATERIAL_MODEL_FEATURE_COUNT] = {{
{c_matrix(coefficients)}
}};

static const float MATERIAL_MODEL_BIAS[MATERIAL_MODEL_CLASS_COUNT] = {{
{float_array(intercept)}
}};

#endif /* {guard} */
"""
    path.write_text(content, encoding="utf-8", newline="\n")


def export_rbf_svm_c_header(
    model: Pipeline,
    features: list[str],
    path: Path,
    dataset_name: str,
) -> None:
    scaler: StandardScaler = model.named_steps["scaler"]
    classifier: SVC = model.named_steps["classifier"]
    classes = [str(value) for value in classifier.classes_]
    guard = f"{dataset_name.upper()}_MATERIAL_MODEL_H".replace("-", "_")
    class_lines = ",\n".join(f'    "{name}"' for name in classes)
    feature_lines = ",\n".join(f'    "{name}"' for name in features)
    support_counts = ", ".join(str(int(value)) for value in classifier.n_support_)
    support_starts = np.concatenate(([0], np.cumsum(classifier.n_support_)[:-1]))
    start_values = ", ".join(str(int(value)) for value in support_starts)
    pair_count = len(classes) * (len(classes) - 1) // 2

    content = f"""/* Auto-generated by train_material_classifier.py. */
#ifndef {guard}
#define {guard}

#include <stdint.h>

#define MATERIAL_MODEL_IS_RBF_SVM 1
#define MATERIAL_MODEL_CLASS_COUNT {len(classes)}
#define MATERIAL_MODEL_FEATURE_COUNT {len(features)}
#define MATERIAL_MODEL_SUPPORT_VECTOR_COUNT {len(classifier.support_vectors_)}
#define MATERIAL_MODEL_PAIR_COUNT {pair_count}

static const char *const MATERIAL_MODEL_CLASSES[MATERIAL_MODEL_CLASS_COUNT] = {{
{class_lines}
}};

static const char *const MATERIAL_MODEL_FEATURES[MATERIAL_MODEL_FEATURE_COUNT] = {{
{feature_lines}
}};

static const float MATERIAL_SCALER_MEAN[MATERIAL_MODEL_FEATURE_COUNT] = {{
{float_array(scaler.mean_)}
}};

static const float MATERIAL_SCALER_SCALE[MATERIAL_MODEL_FEATURE_COUNT] = {{
{float_array(scaler.scale_)}
}};

static const float MATERIAL_SVM_GAMMA = {float_array(np.array([classifier._gamma]), "")};

static const uint16_t MATERIAL_SVM_SUPPORT_COUNT[MATERIAL_MODEL_CLASS_COUNT] = {{
    {support_counts}
}};

static const uint16_t MATERIAL_SVM_SUPPORT_START[MATERIAL_MODEL_CLASS_COUNT] = {{
    {start_values}
}};

static const float MATERIAL_SVM_SUPPORT_VECTORS[MATERIAL_MODEL_SUPPORT_VECTOR_COUNT]
                                               [MATERIAL_MODEL_FEATURE_COUNT] = {{
{c_matrix(classifier.support_vectors_)}
}};

static const float MATERIAL_SVM_DUAL_COEF[MATERIAL_MODEL_CLASS_COUNT - 1]
                                         [MATERIAL_MODEL_SUPPORT_VECTOR_COUNT] = {{
{c_matrix(classifier.dual_coef_)}
}};

static const float MATERIAL_SVM_INTERCEPT[MATERIAL_MODEL_PAIR_COUNT] = {{
{float_array(classifier.intercept_)}
}};

#endif /* {guard} */
"""
    path.write_text(content, encoding="utf-8", newline="\n")


def export_c_header(
    model: Pipeline,
    features: list[str],
    path: Path,
    dataset_name: str,
) -> None:
    classifier = model.named_steps["classifier"]
    if isinstance(classifier, LogisticRegression):
        export_logistic_c_header(model, features, path, dataset_name)
    elif isinstance(classifier, SVC) and classifier.kernel == "rbf":
        export_rbf_svm_c_header(model, features, path, dataset_name)
    else:
        raise ValueError(f"暂不支持导出模型类型: {type(classifier).__name__}")


def rbf_svm_scores(
    classifier: SVC,
    standardized: np.ndarray,
) -> np.ndarray:
    values = np.asarray(standardized, dtype=np.float32)
    support_vectors = np.asarray(classifier.support_vectors_, dtype=np.float32)
    gamma = np.float32(classifier._gamma)
    kernels = np.exp(
        -gamma * np.sum((values[:, None, :] - support_vectors[None, :, :]) ** 2, axis=2)
    ).astype(np.float32)

    class_count = len(classifier.classes_)
    starts = np.concatenate(([0], np.cumsum(classifier.n_support_)))
    votes = np.zeros((len(values), class_count), dtype=np.float32)
    confidence_sums = np.zeros_like(votes)
    dual_coef = np.asarray(classifier.dual_coef_, dtype=np.float32)
    intercept = np.asarray(classifier.intercept_, dtype=np.float32)

    pair_index = 0
    for first_class in range(class_count):
        for second_class in range(first_class + 1, class_count):
            first_slice = slice(starts[first_class], starts[first_class + 1])
            second_slice = slice(starts[second_class], starts[second_class + 1])
            margin = (
                kernels[:, first_slice] @ dual_coef[second_class - 1, first_slice]
                + kernels[:, second_slice] @ dual_coef[first_class, second_slice]
                + intercept[pair_index]
            )
            votes[:, first_class] += margin >= 0.0
            votes[:, second_class] += margin < 0.0
            confidence_sums[:, first_class] += margin
            confidence_sums[:, second_class] -= margin
            pair_index += 1

    return votes + confidence_sums / (3.0 * (np.abs(confidence_sums) + 1.0))


def verify_export(model: Pipeline, data: pd.DataFrame, features: list[str]) -> None:
    scaler: StandardScaler = model.named_steps["scaler"]
    classifier = model.named_steps["classifier"]
    values = data[features].to_numpy(dtype=np.float32)
    standardized = (
        values - np.asarray(scaler.mean_, dtype=np.float32)
    ) / np.asarray(scaler.scale_, dtype=np.float32)

    if isinstance(classifier, LogisticRegression):
        scores = (
            standardized @ np.asarray(classifier.coef_.T, dtype=np.float32)
            + np.asarray(classifier.intercept_, dtype=np.float32)
        )
    elif isinstance(classifier, SVC) and classifier.kernel == "rbf":
        scores = rbf_svm_scores(classifier, standardized)
    else:
        raise ValueError(f"暂不支持复核模型类型: {type(classifier).__name__}")

    manual_predictions = classifier.classes_[np.argmax(scores, axis=1)]
    pipeline_predictions = model.predict(data[features])
    if not np.array_equal(manual_predictions, pipeline_predictions):
        mismatch = int(np.sum(manual_predictions != pipeline_predictions))
        raise RuntimeError(f"float32 C参数无法复现Python预测，差异帧数: {mismatch}")


def save_evaluation_figure(
    results: list[CvResult],
    selected: CvResult,
    selected_model: Pipeline,
    data: pd.DataFrame,
    path: Path,
) -> None:
    labels = sorted(data["label"].astype(str).unique())
    matrix = confusion_matrix(data["label"], selected.predictions, labels=labels)

    figure, axes = plt.subplots(1, 3, figsize=(16, 5))

    image = axes[0].imshow(matrix, cmap="Blues")
    axes[0].set_title(f"{selected.name} confusion matrix")
    axes[0].set_xticks(range(len(labels)), labels, rotation=25, ha="right")
    axes[0].set_yticks(range(len(labels)), labels)
    axes[0].set_xlabel("Predicted")
    axes[0].set_ylabel("True")
    for row in range(len(labels)):
        for column in range(len(labels)):
            axes[0].text(column, row, str(matrix[row, column]), ha="center", va="center")
    figure.colorbar(image, ax=axes[0], fraction=0.046)

    names = [result.name for result in results]
    accuracies = [result.accuracy * 100.0 for result in results]
    bars = axes[1].bar(names, accuracies, color="#2f7ed8")
    axes[1].set_ylim(0, 105)
    axes[1].set_ylabel(f"{selected.validation_group}-held-out accuracy (%)")
    axes[1].set_title("Model comparison")
    axes[1].tick_params(axis="x", rotation=30)
    for bar, accuracy in zip(bars, accuracies):
        axes[1].text(bar.get_x() + bar.get_width() / 2, accuracy + 1, f"{accuracy:.1f}%", ha="center")

    classifier = selected_model.named_steps["classifier"]
    if isinstance(classifier, LogisticRegression):
        coefficient_image = axes[2].imshow(
            classifier.coef_,
            cmap="coolwarm",
            aspect="auto",
        )
        axes[2].set_title("Deployment logistic weights")
        axes[2].set_xticks(
            range(len(selected.features)),
            selected.features,
            rotation=60,
            ha="right",
        )
        axes[2].set_yticks(range(len(classifier.classes_)), classifier.classes_)
        figure.colorbar(coefficient_image, ax=axes[2], fraction=0.046)
    elif isinstance(classifier, SVC):
        support_bars = axes[2].bar(
            classifier.classes_,
            classifier.n_support_,
            color="#f28e2b",
        )
        axes[2].set_title("Deployment RBF-SVM support vectors")
        axes[2].set_ylabel("Support vector count")
        for bar, count in zip(support_bars, classifier.n_support_):
            axes[2].text(
                bar.get_x() + bar.get_width() / 2,
                count + 0.5,
                str(int(count)),
                ha="center",
            )

    figure.suptitle("Classifier evaluation")
    figure.tight_layout()
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=180)
    plt.close(figure)


def main() -> int:
    args = parse_args()
    input_path = resolve_input_path(args.input)
    if not input_path.exists():
        raise FileNotFoundError(input_path)

    dataset_name = args.dataset_name or infer_dataset_name(input_path)
    data = prepare_data(input_path)
    validation_group = choose_validation_group(data, args.group_column)
    models = build_models()
    feature_sets = {
        "raw": RAW_FEATURES,
        "normalized": NORMALIZED_FEATURES,
    }

    results: list[CvResult] = []
    for model_name, model in models.items():
        for feature_set, features in feature_sets.items():
            results.append(
                cross_validate_by_group(
                    data,
                    model,
                    model_name,
                    feature_set,
                    features,
                    validation_group,
                )
            )

    deployable_results = [
        result
        for result in results
        if result.model_name in {"logistic", "svm_rbf"}
    ]
    selected = max(deployable_results, key=lambda result: result.accuracy)

    deployment_model: Pipeline = clone(models[selected.model_name])
    deployment_model.fit(data[selected.features], data["label"])
    verify_export(deployment_model, data, selected.features)

    logistic_model = clone(models["logistic"])
    logistic_model.fit(data[RAW_FEATURES], data["label"])
    forest_model = clone(models["random_forest"])
    forest_model.fit(data[RAW_FEATURES], data["label"])
    svm_model = clone(models["svm_rbf"])
    svm_model.fit(data[RAW_FEATURES], data["label"])

    MODELS_DIR.mkdir(parents=True, exist_ok=True)
    FIGURES_DIR.mkdir(parents=True, exist_ok=True)
    logistic_path = MODELS_DIR / f"{dataset_name}_logistic.joblib"
    forest_path = MODELS_DIR / f"{dataset_name}_random_forest.joblib"
    svm_path = MODELS_DIR / f"{dataset_name}_svm_rbf.joblib"
    deployment_path = MODELS_DIR / f"{dataset_name}_deployment.joblib"
    header_path = MODELS_DIR / f"{dataset_name}_material_model.h"
    metrics_path = MODELS_DIR / f"{dataset_name}_cv_metrics.csv"
    metadata_path = MODELS_DIR / f"{dataset_name}_model_metadata.json"
    figure_path = FIGURES_DIR / f"{dataset_name}_model_evaluation.png"

    joblib.dump(logistic_model, logistic_path)
    joblib.dump(forest_model, forest_path)
    joblib.dump(svm_model, svm_path)
    joblib.dump(deployment_model, deployment_path)
    export_c_header(deployment_model, selected.features, header_path, dataset_name)
    esp_header_path: Path | None = None
    if args.esp_header:
        esp_header_path = Path(args.esp_header)
        if not esp_header_path.is_absolute():
            esp_header_path = PROJECT_ROOT / esp_header_path
        esp_header_path.parent.mkdir(parents=True, exist_ok=True)
        export_c_header(
            deployment_model,
            selected.features,
            esp_header_path,
            dataset_name,
        )

    metric_rows = [
        {
            "model": result.model_name,
            "feature_set": result.feature_set,
            "accuracy": result.accuracy,
        }
        for result in results
    ]
    pd.DataFrame(metric_rows).to_csv(metrics_path, index=False, encoding="utf-8-sig")

    report = classification_report(
        data["label"],
        selected.predictions,
        output_dict=True,
        zero_division=0,
    )
    metadata = {
        "dataset": str(input_path),
        "dataset_name": dataset_name,
        "frames": len(data),
        "labels": sorted(data["label"].astype(str).unique()),
        "positions": sorted(str(value) for value in data["position"].unique()),
        "sample_ids": sorted(data["sample_id"].astype(str).unique()),
        "validation_group": validation_group,
        "selected_model": selected.model_name,
        "selected_feature_set": selected.feature_set,
        "features": selected.features,
        "group_held_out_accuracy": selected.accuracy,
        "classification_report": report,
        "all_results": metric_rows,
        "folds": selected.fold_rows,
    }
    selected_classifier = deployment_model.named_steps["classifier"]
    if isinstance(selected_classifier, SVC):
        metadata["rbf_svm"] = {
            "C": float(selected_classifier.C),
            "gamma": float(selected_classifier._gamma),
            "support_vectors": int(len(selected_classifier.support_vectors_)),
            "support_vectors_per_class": [
                int(value) for value in selected_classifier.n_support_
            ],
        }
    metadata_path.write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2),
        encoding="utf-8",
    )
    save_evaluation_figure(results, selected, deployment_model, data, figure_path)

    print("=== 光谱分类模型训练 ===")
    print(f"数据集: {input_path}")
    print(f"有效帧: {len(data)}")
    print(f"类别: {', '.join(sorted(data['label'].astype(str).unique()))}")
    print(f"\n按完整 {validation_group} 留出验证:")
    if validation_group == "position":
        print("  [提示] 当前每类独立样品不足2个，结果只代表现有样品的不同位置。")
    for result in results:
        print(f"  {result.name:<28} {result.accuracy * 100:6.2f}%")
    print(f"\n部署模型: {selected.name}")
    print(f"部署准确率: {selected.accuracy * 100:.2f}%")
    print(f"特征顺序: {', '.join(selected.features)}")
    print(f"\n逻辑回归模型: {logistic_path}")
    print(f"随机森林模型: {forest_path}")
    print(f"RBF-SVM模型: {svm_path}")
    print(f"最终部署模型: {deployment_path}")
    print(f"ESP32头文件: {header_path}")
    if esp_header_path is not None:
        print(f"ESP工程头文件: {esp_header_path}")
    print(f"指标CSV: {metrics_path}")
    print(f"元数据JSON: {metadata_path}")
    print(f"评估图片: {figure_path}")
    print("导出复核: C参数与Python预测一致")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, ValueError, pd.errors.ParserError) as error:
        print(f"[错误] {error}")
        raise SystemExit(1)
