"""æ•´ç†æ°´æžœæ–°é²œåº¦è¿žç»­å®žéªŒæ•°æ®ï¼Œå¹¶åˆ†æžç›¸å¯¹ Day 0 çš„å˜åŒ–ã€‚"""

from __future__ import annotations

import argparse
import os
from pathlib import Path
from typing import Any


PROJECT_ROOT = Path(__file__).resolve().parents[1]
os.environ.setdefault(
    "MPLCONFIGDIR",
    str(PROJECT_ROOT / ".cache" / "matplotlib"),
)

import matplotlib.pyplot as plt  # noqa: E402
from matplotlib.ticker import MaxNLocator  # noqa: E402
import numpy as np  # noqa: E402
import pandas as pd  # noqa: E402


RAW_DIR = PROJECT_ROOT / "data" / "raw"
PROCESSED_DIR = PROJECT_ROOT / "data" / "processed"
FIGURE_DIR = PROJECT_ROOT / "figures"
DEFAULT_METADATA_CORRECTIONS_PATH = (
    PROJECT_ROOT / "config" / "freshness_metadata_corrections.csv"
)
DEFAULT_SELECTION_OVERRIDES_PATH = (
    PROJECT_ROOT / "config" / "freshness_file_selection_overrides.csv"
)

VISIBLE_CHANNELS = [
    "F1_415nm",
    "F2_445nm",
    "F3_480nm",
    "F4_515nm",
    "F5_555nm",
    "F6_590nm",
    "F7_630nm",
    "F8_680nm",
]
SPECTRAL_CHANNELS = [*VISIBLE_CHANNELS, "Clear", "NIR"]
WAVELENGTHS_NM = np.array([415, 445, 480, 515, 555, 590, 630, 680])
STATE_ORDER = {"fresh": 0, "warning": 1, "spoiled": 2}
CORRECTABLE_METADATA_FIELDS = {
    "freshness_state",
    "temperature_c",
    "weight_g",
    "firmness_score",
    "surface_note",
}

REQUIRED_COLUMNS = {
    "timestamp",
    "experiment_mode",
    "sample_id",
    "position",
    "fruit_type",
    "storage_day",
    "freshness_state",
    "temperature_c",
    "weight_g",
    "firmness_score",
    "surface_note",
    "distance_mm",
    "distance_in_range",
    "reflectance_valid",
    "quality_valid",
    *SPECTRAL_CHANNELS,
}


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="æ•´ç†æ°´æžœæ–°é²œåº¦è¿žç»­å®žéªŒï¼Œå¹¶ç”Ÿæˆè´¨é‡ã€Day 0åŸºçº¿å’Œå˜åŒ–è¶‹åŠ¿æŠ¥å‘Šã€‚"
    )
    parser.add_argument("--raw-dir", default=str(RAW_DIR), help="åŽŸå§‹CSVç›®å½•")
    parser.add_argument("--fruit-type", default="nectarine", help="æ°´æžœç±»åž‹")
    parser.add_argument(
        "--output-prefix",
        default="NECT_FRESHNESS",
        help="è¾“å‡ºæ–‡ä»¶åå‰ç¼€",
    )
    parser.add_argument(
        "--positions",
        nargs="+",
        default=["1", "2", "3", "4"],
        help="æ¯å¤©æœŸæœ›é‡‡é›†çš„ä½ç½®ç¼–å·",
    )
    parser.add_argument(
        "--frames-per-position",
        type=int,
        default=20,
        help="æ¯ä¸ªä½ç½®è¦æ±‚çš„åˆæ ¼å¸§æ•°",
    )
    parser.add_argument(
        "--cv-warning-percent",
        type=float,
        default=5.0,
        help="é€šé“CVä¸­ä½æ•°è¶…è¿‡è¯¥å€¼æ—¶å‘å‡ºç¨³å®šæ€§è­¦å‘Š",
    )
    parser.add_argument(
        "--weight-unavailable",
        action="store_true",
        help="é‡é‡æœªå®žé™…æµ‹é‡æ—¶å¿½ç•¥CSVä¸­çš„å ä½å€¼ï¼Œä¸è®¡ç®—é‡é‡æŸå¤±",
    )
    parser.add_argument(
        "--metadata-corrections",
        default=str(DEFAULT_METADATA_CORRECTIONS_PATH),
        help="å¯è¿½æº¯å…ƒæ•°æ®æ›´æ­£CSVï¼›é»˜è®¤è¯»å–config/freshness_metadata_corrections.csv",
    )
    parser.add_argument(
        "--selection-overrides",
        default=str(DEFAULT_SELECTION_OVERRIDES_PATH),
        help="Auditable CSV overrides for intentionally selected retest files",
    )
    return parser.parse_args()


def coefficient_of_variation(data: pd.DataFrame) -> pd.Series:
    means = data[SPECTRAL_CHANNELS].mean()
    stds = data[SPECTRAL_CHANNELS].std(ddof=1).fillna(0.0)
    return (stds / means.replace(0, np.nan) * 100.0).replace(
        [np.inf, -np.inf], np.nan
    )


def validate_single_value(data: pd.DataFrame, column: str) -> Any:
    values = data[column].dropna().unique()
    if len(values) != 1:
        raise ValueError(f"å­—æ®µ {column} åœ¨åŒä¸€æ–‡ä»¶ä¸­æœ‰ {len(values)} ä¸ªä¸åŒå–å€¼")
    return values[0]


def build_candidate_record(
    data: pd.DataFrame,
    source_file: str,
    expected_frames: int,
) -> dict[str, Any]:
    """ä¸ºä¸€ä»½åŽŸå§‹CSVç”Ÿæˆå¯å®¡è®¡çš„è´¨é‡è®°å½•ã€‚"""
    missing = sorted(REQUIRED_COLUMNS.difference(data.columns))
    if missing:
        raise ValueError(f"ç¼ºå°‘å­—æ®µ: {', '.join(missing)}")

    valid = data.loc[pd.to_numeric(data["quality_valid"], errors="coerce") == 1]
    cv = coefficient_of_variation(valid) if not valid.empty else pd.Series(dtype=float)
    distance = pd.to_numeric(valid["distance_mm"], errors="coerce").dropna()
    timestamps = pd.to_datetime(data["timestamp"], errors="coerce").dropna()

    return {
        "source_file": source_file,
        "sample_id": str(validate_single_value(data, "sample_id")),
        "storage_day": int(validate_single_value(data, "storage_day")),
        "position": str(validate_single_value(data, "position")),
        "collection_started_at": timestamps.min().isoformat()
        if not timestamps.empty
        else "",
        "collection_ended_at": timestamps.max().isoformat()
        if not timestamps.empty
        else "",
        "freshness_state": str(validate_single_value(data, "freshness_state")),
        "fruit_type": str(validate_single_value(data, "fruit_type")),
        "raw_frames": int(len(data)),
        "valid_frames": int(len(valid)),
        "quality_rate_percent": float(len(valid) / len(data) * 100.0)
        if len(data)
        else 0.0,
        "complete": int(len(valid) >= expected_frames),
        "distance_mean_mm": float(distance.mean()) if not distance.empty else np.nan,
        "distance_std_mm": float(distance.std(ddof=1))
        if len(distance) > 1
        else 0.0,
        "distance_min_mm": float(distance.min()) if not distance.empty else np.nan,
        "distance_max_mm": float(distance.max()) if not distance.empty else np.nan,
        "median_cv_percent": float(cv.median()) if not cv.empty else np.nan,
        "max_cv_percent": float(cv.max()) if not cv.empty else np.nan,
        "worst_cv_channel": str(cv.idxmax()) if not cv.empty else "",
    }


def select_best_candidates(candidates: pd.DataFrame) -> pd.DataFrame:
    """åœ¨åŒä¸€æ ·å“ã€å¤©æ•°ã€ä½ç½®çš„å¤šæ¬¡é‡‡é›†ä¸­é€‰æ‹©æœ€å®Œæ•´ä¸”æœ€ç¨³å®šçš„ä¸€ä»½ã€‚"""
    result = candidates.copy()
    result["selected"] = 0
    result["selection_reason"] = "excluded_duplicate"
    group_columns = ["sample_id", "storage_day", "position"]

    for _, group in result.groupby(group_columns, sort=False):
        ranked = group.sort_values(
            ["complete", "median_cv_percent", "valid_frames", "source_file"],
            ascending=[False, True, False, False],
            na_position="last",
        )
        winner = ranked.index[0]
        result.loc[winner, "selected"] = 1
        result.loc[winner, "selection_reason"] = (
            "selected_complete_lowest_cv"
            if result.loc[winner, "complete"]
            else "selected_incomplete_best_available"
        )

    return result.sort_values(
        ["sample_id", "storage_day", "position", "selected", "source_file"],
        ascending=[True, True, True, False, True],
    ).reset_index(drop=True)


def apply_selection_overrides(
    selection: pd.DataFrame,
    overrides_path: Path,
) -> pd.DataFrame:
    """Apply explicit source-file choices after automatic quality ranking."""
    result = selection.copy()
    result["selection_override_reason"] = ""
    if not overrides_path.exists():
        return result

    overrides = pd.read_csv(overrides_path, dtype=str).fillna("")
    required = {"source_file", "reason"}
    missing = sorted(required.difference(overrides.columns))
    if missing:
        raise ValueError(
            f"Selection override CSV is missing columns: {', '.join(missing)}"
        )

    group_columns = ["sample_id", "storage_day", "position"]
    for override in overrides.itertuples(index=False):
        source_file = str(override.source_file).strip()
        source_mask = result["source_file"].astype(str).eq(source_file)
        if int(source_mask.sum()) != 1:
            raise ValueError(
                f"Selection override must match exactly one candidate: {source_file}"
            )

        winner = result.loc[source_mask].iloc[0]
        group_mask = pd.Series(True, index=result.index)
        for column in group_columns:
            group_mask &= result[column].astype(str).eq(str(winner[column]))

        reason = str(override.reason).strip()
        result.loc[group_mask, "selected"] = 0
        result.loc[group_mask, "selection_reason"] = "excluded_by_manual_override"
        result.loc[group_mask, "selection_override_reason"] = reason
        result.loc[source_mask, "selected"] = 1
        result.loc[source_mask, "selection_reason"] = "selected_manual_override"

    return result


def discover_candidates(
    raw_dir: Path,
    fruit_type: str,
    expected_frames: int,
) -> tuple[pd.DataFrame, dict[str, pd.DataFrame], list[str]]:
    records: list[dict[str, Any]] = []
    frames: dict[str, pd.DataFrame] = {}
    skipped: list[str] = []

    for path in sorted(raw_dir.glob("*.csv")):
        try:
            data = pd.read_csv(path)
        except (OSError, UnicodeDecodeError, pd.errors.ParserError) as error:
            skipped.append(f"{path.name}: æ— æ³•è¯»å–ï¼ˆ{error}ï¼‰")
            continue

        if not REQUIRED_COLUMNS.issubset(data.columns):
            continue
        selected = data.loc[
            (data["experiment_mode"].astype(str) == "freshness")
            & (data["fruit_type"].astype(str) == fruit_type)
        ].copy()
        if selected.empty:
            continue

        try:
            record = build_candidate_record(selected, path.name, expected_frames)
        except (TypeError, ValueError) as error:
            skipped.append(f"{path.name}: {error}")
            continue

        records.append(record)
        frames[path.name] = selected

    if not records:
        raise FileNotFoundError(
            f"{raw_dir} ä¸­æ²¡æœ‰ fruit_type={fruit_type!r} çš„æ–°é²œåº¦å®žéªŒCSV"
        )
    return pd.DataFrame(records), frames, skipped


def build_curated_frames(
    selection: pd.DataFrame,
    source_frames: dict[str, pd.DataFrame],
    expected_frames: int,
) -> pd.DataFrame:
    curated: list[pd.DataFrame] = []
    selected_rows = selection.loc[selection["selected"] == 1]

    for row in selected_rows.itertuples(index=False):
        data = source_frames[row.source_file]
        valid = data.loc[data["quality_valid"] == 1].copy().head(expected_frames)
        valid["selected_source_file"] = row.source_file
        curated.append(valid)

    if not curated:
        raise ValueError("æ²¡æœ‰é€‰å‡ºå¯ç”¨çš„æ–°é²œåº¦æ•°æ®")
    result = pd.concat(curated, ignore_index=True)
    result["storage_day"] = pd.to_numeric(result["storage_day"]).astype(int)
    result["position"] = result["position"].astype(str)
    return result.sort_values(
        ["sample_id", "storage_day", "position", "timestamp"]
    ).reset_index(drop=True)


def apply_metadata_corrections(
    curated: pd.DataFrame,
    corrections_path: Path,
) -> tuple[pd.DataFrame, pd.DataFrame]:
    """é€šè¿‡ç‹¬ç«‹å®¡è®¡è¡¨ä¿®æ­£è¯¯è¾“å…¥å…ƒæ•°æ®ï¼Œä¸æ”¹å†™åŽŸå§‹CSVã€‚"""
    result = curated.copy()
    result["metadata_corrected"] = 0
    result["metadata_correction_notes"] = ""
    if not corrections_path.exists():
        return result, pd.DataFrame()

    corrections = pd.read_csv(corrections_path, dtype=str).fillna("")
    required = {"source_file", "field_name", "corrected_value", "reason"}
    missing = sorted(required.difference(corrections.columns))
    if missing:
        raise ValueError(
            f"å…ƒæ•°æ®æ›´æ­£è¡¨ç¼ºå°‘å­—æ®µ: {', '.join(missing)}"
        )

    audit_rows: list[dict[str, Any]] = []
    for correction in corrections.itertuples(index=False):
        field_name = str(correction.field_name).strip()
        if field_name not in CORRECTABLE_METADATA_FIELDS:
            raise ValueError(f"ä¸å…è®¸æ›´æ­£å­—æ®µ: {field_name}")
        mask = result["selected_source_file"].astype(str) == str(
            correction.source_file
        ).strip()
        matched_frames = int(mask.sum())
        if matched_frames == 0:
            raise ValueError(
                f"å…ƒæ•°æ®æ›´æ­£æ²¡æœ‰åŒ¹é…é€‰ç”¨æ–‡ä»¶: {correction.source_file}"
            )

        old_values = sorted(set(result.loc[mask, field_name].astype(str)))
        raw_value = str(correction.corrected_value).strip()
        if field_name == "firmness_score":
            corrected_value: Any = int(float(raw_value))
        elif field_name in {"temperature_c", "weight_g"}:
            corrected_value = float(raw_value) if raw_value else np.nan
        else:
            corrected_value = raw_value

        result.loc[mask, field_name] = corrected_value
        result.loc[mask, "metadata_corrected"] = 1
        note = (
            f"{field_name}: {' | '.join(old_values)} -> {raw_value}"
            f" ({str(correction.reason).strip()})"
        )
        existing_notes = result.loc[mask, "metadata_correction_notes"].astype(str)
        result.loc[mask, "metadata_correction_notes"] = existing_notes.where(
            existing_notes == "", existing_notes + "; "
        ) + note
        audit_rows.append(
            {
                "source_file": str(correction.source_file).strip(),
                "field_name": field_name,
                "original_value": " | ".join(old_values),
                "corrected_value": raw_value,
                "reason": str(correction.reason).strip(),
                "matched_frames": matched_frames,
            }
        )
    return result, pd.DataFrame(audit_rows)


def build_completeness_report(
    selection: pd.DataFrame,
    positions: list[str],
) -> pd.DataFrame:
    selected = selection.loc[selection["selected"] == 1].copy()
    rows: list[dict[str, Any]] = []

    for sample_id, sample in selected.groupby("sample_id"):
        minimum_day = int(sample["storage_day"].min())
        maximum_day = int(sample["storage_day"].max())
        for day in range(minimum_day, maximum_day + 1):
            for position in positions:
                match = sample.loc[
                    (sample["storage_day"] == day)
                    & (sample["position"].astype(str) == str(position))
                ]
                if match.empty:
                    rows.append(
                        {
                            "sample_id": sample_id,
                            "storage_day": day,
                            "position": str(position),
                            "status": "missing",
                            "valid_frames": 0,
                            "median_cv_percent": np.nan,
          ß½;¶‰žËkºwµç@È¸À°€‹’â7–îë¢º»žnÓš:—–¢ºû¢«žRÇž¦ë¦^Ó–æÏšZç–>7š¾Pˆ¤°4(€€€€€€€€ ‰¥¹}Í…µÁ±•}™¥ÑÑ•ˆ°™¥ÑÑ•‘}•áÁ½¹•¹Ð°€‹’î’úo¢ž–¾¾ò3’â7¢÷šnÿ’îž.³ž®/¢Þwžšïš‚–ºhˆ¤°4(€€€t4(€€€É½ÝÌè±¥ÍÑm‘¥ÑmÍÑÈ°¹åut€ômt4(€€€‰…Í•±¥¹•}µ•‘¥…¹}Ø€ô¹À¹¹…¸4(€€€™½Èµ•Ñ¡½°•áÁ½¹•¹Ð°¹½Ñ”¥¸µ•Ñ¡½‘Ìè4(€€€€€€€½ÉÉ•Ñ•€ôÕÍ…‰±•l‰±•…É}µ•…¸‰t€¨€ 4(€€€€€€€€€€€ÕÍ…‰±•l‰‘¥ÍÑ…¹•}µ•…¹}µ´‰t€¼É•™•É•¹•}‘¥ÍÑ…¹•}µ´4(€€€€€€€€¤€¨¨•áÁ½¹•¹Ð4(€€€€€€€Ñ•µÁ½É…Éä€ôÕÍ…‰±•mÉ½ÕÁ}½±Õµ¹Ít¹½Áä ¤4(€€€€€€€Ñ•µÁ½É…Éål‰½ÉÉ•Ñ•‘}±•…È‰t€ô½ÉÉ•Ñ•4(€€€€€€€É½ÕÁ•€ôÑ•µÁ½É…Éä¹É½ÕÁ‰ä¡É½ÕÁ}½±Õµ¹Ì¥l‰½ÉÉ•Ñ•‘}±•…È‰t4(€€€€€€€ÙÌ€ôÉ½ÕÁ•¹ÍÑ¡‘‘½˜ôÄ¤€¼É½ÕÁ•¹µ•…¸ ¤€¨€ÄÀÀ¸À4(€€€€€€€µ•‘¥…¹}Ø€ô™±½…Ð¡ÙÌ¹µ•‘¥…¸ ¤¤4(€€€€€€€¥˜µ•Ñ¡½€ôô€‰¹½¹”ˆè4(€€€€€€€€€€€‰…Í•±¥¹•}µ•‘¥…¹}Ø€ôµ•‘¥…¹}Ø4(€€€€€€€¥µÁÉ½Ù•µ•¹Ð€ô€ 4(€€€€€€€€€€€€¡‰…Í•±¥¹•}µ•‘¥…¹}Ø€´µ•‘¥…¹}Ø¤€¼‰…Í•±¥¹•}µ•‘¥…¹}Ø€¨€ÄÀÀ¸À4(€€€€€€€€€€€¥˜‰…Í•±¥¹•}µ•‘¥…¹}Ø4(€€€€€€€€€€€•±Í”¹À¹¹…¸4(€€€€€€€€¤4(€€€€€€€É½ÝÌ¹…ÁÁ•¹ 4(€€€€€€€€€€€ì4(€€€€€€€€€€€€€€€€‰µ•Ñ¡½ˆèµ•Ñ¡½°4(€€€€€€€€€€€€€€€€‰•áÁ½¹•¹Ðˆè•áÁ½¹•¹Ð°4(€€€€€€€€€€€€€€€€‰É•™•É•¹•}‘¥ÍÑ…¹•}µ´ˆèÉ•™•É•¹•}‘¥ÍÑ…¹•}µ´°4(€€€€€€€€€€€€€€€€‰µ•‘¥…¹}Á½Í¥Ñ¥½¹}Ù}Á•É•¹Ðˆèµ•‘¥…¹}Ø°4(€€€€€€€€€€€€€€€€‰µ•…¹}Á½Í¥Ñ¥½¹}Ù}Á•É•¹Ðˆè™±½…Ð¡ÙÌ¹µ•…¸ ¤¤°4(€€€€€€€€€€€€€€€€‰¥µÁÉ½Ù•µ•¹Ñ}ÙÍ}¹½¹•}Á•É•¹Ðˆè¥µÁÉ½Ù•µ•¹Ð°4(€€€€€€€€€€€€€€€€‰É•½µµ•¹‘•‘}™½É}ØÄˆè¥¹Ð¡µ•Ñ¡½€ôô€‰¹½¹”ˆ¤°4(€€€€€€€€€€€€€€€€‰¹½Ñ”ˆè¹½Ñ”°4(€€€€€€€€€€€ô4(€€€€€€€€¤4(€€€É•ÑÕÉ¸Á¹…Ñ…É…µ”¡É½ÝÌ¤4(4(4)‘•˜…ÍÍ•ÍÍ}µ½‘•±}É•…‘¥¹•ÍÌ 4(€€€‘…¥±å}µ½‘•±}Í…µÁ±•ÌèÁ¹…Ñ…É…µ”°4(€€€µ¥¹¥µÕµ}™ÉÕ¥ÑÍ}Á•É}ÍÑ…Ñ”è¥¹Ð€ô€È°4(¤€´øÑÕÁ±•m‰½½°°ÍÑÉtè4(€€€€ˆˆ‹–"–"¯–"“šZµ™É•Í ½É¥Í¯’ê3–"žÆï–J3’â'–"žÆïžjšr’ö;ž.³ž®/šÂÓšzsšv‡’îÛŽˆˆˆ4(€€€™ÉÕ¥Ñ}½Õ¹ÑÌ€ô€ 4(€€€€€€€‘…¥±å}µ½‘•±}Í…µÁ±•Ì¹É½ÕÁ‰ä ‰™É•Í¡¹•ÍÍ}ÍÑ…Ñ”ˆ¥l‰Í…µÁ±•}¥‰t4(€€€€€€€€¹¹Õ¹¥ÅÕ” ¤4(€€€€€€€€¹Í½ÉÑ}¥¹‘•à ¤4(€€€€¤4(€€€¥˜±•¸¡™ÉÕ¥Ñ}½Õ¹ÑÌ¤€ð€Èè4(€€€€€€€É•ÑÕÉ¸…±Í”°€‹–öO–&7–>«šr'’â’â«šZÃ¦Ês–ê›žÆï–"¯¾ò3–#žîŸžî·¢ž–¾–>c–2[Žˆ4(4(€€€½Õ¹ÑÍ}Ñ•áÐ€ô€ˆ°€ˆ¹©½¥¸ 4(€€€€€€€˜‰íÍÑ…Ñ•ôõí¥¹Ð¡½Õ¹Ð¥÷–>«šÂÓšzpˆ™½ÈÍÑ…Ñ”°½Õ¹Ð¥¸™ÉÕ¥Ñ}½Õ¹ÑÌ¹¥Ñ•µÌ ¤4(€€€€¤4(€€€™É•Í¡}™ÉÕ¥ÑÌ€ô¥¹Ð¡™ÉÕ¥Ñ}½Õ¹ÑÌ¹•Ð ‰™É•Í ˆ°€À¤¤4(€€€É¥Í­}™ÉÕ¥ÑÌ€ô¥¹Ð 4(€€€€€€€‘…¥±å}µ½‘•±}Í…µÁ±•Ì¹±½l4(€€€€€€€€€€€‘…¥±å}µ½‘•±}Í…µÁ±•Íl‰™É•Í¡¹•ÍÍ}ÍÑ…Ñ”‰t¹¥Í¥¸¡l‰Ý…É¹¥¹œˆ°€‰ÍÁ½¥±•‰t¤°4(€€€€€€€€€€€€‰Í…µÁ±•}¥ˆ°4(€€€€€€€t¹¹Õ¹¥ÅÕ” ¤4(€€€€¤4(€€€‰¥¹…Éå}É•…‘ä€ô€ 4(€€€€€€€™É•Í¡}™ÉÕ¥ÑÌ€øôµ¥¹¥µÕµ}™ÉÕ¥ÑÍ}Á•É}ÍÑ…Ñ”4(€€€€€€€…¹É¥Í­}™ÉÕ¥ÑÌ€øôµ¥¹¥µÕµ}™ÉÕ¥ÑÍ}Á•É}ÍÑ…Ñ”4(€€€€¤4(€€€Ñ¡É••}±…ÍÍ}É•…‘ä€ô€ 4(€€€€€€€ì‰™É•Í ˆ°€‰Ý…É¹¥¹œˆ°€‰ÍÁ½¥±•‰ô¹¥ÍÍÕ‰Í•Ð¡™ÉÕ¥Ñ}½Õ¹ÑÌ¹¥¹‘•à¤4(€€€€€€€…¹…±° 4(€€€€€€€€€€€¥¹Ð¡™ÉÕ¥Ñ}½Õ¹ÑÌ¹•Ð¡ÍÑ…Ñ”°€À¤¤€øôµ¥¹¥µÕµ}™ÉÕ¥ÑÍ}Á•É}ÍÑ…Ñ”4(€€€€€€€€€€€™½ÈÍÑ…Ñ”¥¸€ ‰™É•Í ˆ°€‰Ý…É¹¥¹œˆ°€‰ÍÁ½¥±•ˆ¤4(€€€€€€€€¤4(€€€€¤4(4(€€€¥˜¹½Ð‰¥¹…Éå}É•…‘äè4(€€€€€€€É•ÑÕÉ¸€ 4(€€€€€€€€€€€…±Í”°4(€€€€€€€€€€€˜‹–ÞËšr'–’k’â«žÆï–"¯¾ò3’öž.³ž®/šÂÓšzs’â7¢ÚÏ¾ò!í½Õ¹ÑÍ}Ñ•áÑ÷¾ò'¾òlˆ4(€€€€€€€€€€€˜‰™É•Í£–J1É¥Í¯–B¢Ï–ÂG¦r¢šíµ¥¹¥µÕµ}™ÉÕ¥ÑÍ}Á•É}ÍÑ…Ñ•÷–>«šÂÓšzsŽˆ°4(€€€€€€€€¤4(€€€¥˜Ñ¡É••}±…ÍÍ}É•…‘äè4(€€€€€€€É•ÑÕÉ¸QÉÕ”°˜‹’ê3–"žÆï–J3’â'–"žÆï–v–ß–’šr’ö;¢º·žîšv‡’îÛ¾ò!í½Õ¹ÑÍ}Ñ•áÑ÷¾ò'Žˆ4(€€€¥˜€‰ÍÁ½¥±•ˆ¥¸™ÉÕ¥Ñ}½Õ¹ÑÌ¹¥¹‘•àè4(€€€€€€€É•ÑÕÉ¸€ 4(€€€€€€€€€€€QÉÕ”°4(€€€€€€€€€€€˜‰™É•Í ½É¥Í¯’ê3–"žÆï–>¿¢º·žî¾ò!™É•Í õí™É•Í¡}™ÉÕ¥ÑÍ÷–>«¾ò1É¥Í¬õíÉ¥Í­}™ÉÕ¥ÑÍ÷–>«¾ò'¾òlˆ4(€€€€€€€€€€€˜‹’â'–"žÆïšj’â7–>¿¢º·žî¾ò!í½Õ¹ÑÍ}Ñ•áÑ÷¾ò'Žˆ°4(€€€€€€€€¤4(€€€É•ÑÕÉ¸QÉÕ”°˜‹–ß–’ž²³’âž&!™É•Í ½Ý…É¹¥¹Ÿ’ê3–"žÆï¢º·žîšv‡’îÛ¾ò!í½Õ¹ÑÍ}Ñ•áÑ÷¾ò'Žˆ4(4(4)‘•˜Í…Ù•}‘…äÁ}‰…Í•±¥¹•}Á±½Ð¡ÍÕµµ…ÉäèÁ¹…Ñ…É…µ”°Á…Ñ èA…Ñ ¤€´ø9½¹”è4(€€€‘…å}é•É¼€ôÍÕµµ…Éä¹±½mÍÕµµ…Éål‰ÍÑ½É…•}‘…ä‰t€ôô€Át4(€€€™¥ÕÉ”°…á•Ì€ôÁ±Ð¹ÍÕ‰Á±½ÑÌ Ä°€È°™¥Í¥é”ô ÄÌ°€Ô¤°½¹ÍÑÉ…¥¹•‘}±…å½ÕÐõQÉÕ”¤4(4(€€€™½ÈÉ½Ü¥¸‘…å}é•É¼¹¥Ñ•ÉÑÕÁ±•Ì¡¥¹‘•àõ…±Í”¤è4(€€€€€€€É…Ü€ô¹À¹…ÉÉ…ä¡m•Ñ…ÑÑÈ¡É½Ü°˜‰í¡…¹¹•±õ}µ•…¸ˆ¤™½È¡…¹¹•°¥¸Y%M%	1}!991Mt¤4(€€€€€€€¹½Éµ…±¥é•€ô¹À¹…ÉÉ…ä 4(€€€€€€€€€€€m•Ñ…ÑÑÈ¡É½Ü°˜‰í¡…¹¹•±õ}±•…É}¹½É´ˆ¤™½È¡…¹¹•°¥¸Y%M%	1}!991Mt4(€€€€€€€€¤4(€€€€€€€…á•ÍlÁt¹Á±½Ð¡]Y19Q!M}94°É…Ü°µ…É­•Èô‰¼ˆ°±…‰•°õÉ½Ü¹Í…µÁ±•}¥¤4(€€€€€€€…á•ÍlÅt¹Á±½Ð¡]Y19Q!M}94°¹½Éµ…±¥é•°µ…É­•Èô‰¼ˆ°±…‰•°õÉ½Ü¹Í…µÁ±•}¥¤4(4(€€€…á•ÍlÁt¹Í•Ñ}Ñ¥Ñ±” ‰…ä€ÀÉ…ÜÉ•™±•Ñ…¹”‰…Í•±¥¹”ˆ¤4(€€€…á•ÍlÁt¹Í•Ñ}å±…‰•° ‰5•…¸É…Ü½Õ¹Ðˆ¤4(€€€…á•ÍlÅt¹Í•Ñ}Ñ¥Ñ±” ‰…ä€À±•…Èµ¹½Éµ…±¥é•‰…Í•±¥¹”ˆ¤4(€€€…á•ÍlÅt¹Í•Ñ}å±…‰•° ‰¡…¹¹•°€¼±•…Èˆ¤4(€€€™½È…á¥Ì¥¸…á•Ìè4(€€€€€€€…á¥Ì¹Í•Ñ}á±…‰•° ‰]…Ù•±•¹Ñ €¡¹´¤ˆ¤4(€€€€€€€…á¥Ì¹É¥¡…±Á¡„ôÀ¸ÈÔ¤4(€€€€€€€¥˜¹½Ð‘…å}é•É¼¹•µÁÑäè4(€€€€€€€€€€€…á¥Ì¹±••¹ ¤4(€€€™¥ÕÉ”¹ÍÕÁÑ¥Ñ±” ‰9•Ñ…É¥¹”™É•Í¡¹•ÍÌ•áÁ•É¥µ•¹Ð€´…ä€Àˆ¤4(€€€Á…Ñ ¹Á…É•¹Ð¹µ­‘¥È¡Á…É•¹ÑÌõQÉÕ”°•á¥ÍÑ}½¬õQÉÕ”¤4(€€€™¥ÕÉ”¹Í…Ù•™¥œ¡Á…Ñ °‘Á¤ôÄàÀ¤4(€€€Á±Ð¹±½Í”¡™¥ÕÉ”¤4(4(4)‘•˜Í…Ù•}ÑÉ…©•Ñ½Éå}Á±½Ð¡ÍÕµµ…ÉäèÁ¹…Ñ…É…µ”°Á…Ñ èA…Ñ ¤€´ø9½¹”è4(€€€™¥ÕÉ”°…á•Ì€ôÁ±Ð¹ÍÕ‰Á±½ÑÌ È°€È°™¥Í¥é”ô ÄÌ°€à¤°½¹ÍÑÉ…¥¹•‘}±…å½ÕÐõQÉÕ”¤4(€€€ÍÑ½É…•}‘…åÌ€ôÍ½ÉÑ•¡¥¹Ð¡Ù…±Õ”¤™½ÈÙ…±Õ”¥¸ÍÕµµ…Éål‰ÍÑ½É…•}‘…ä‰t¹Õ¹¥ÅÕ” ¤¤4(€€€µ•ÑÉ¥Ì€ôl4(€€€€€€€€ ‰±•…É}ÙÍ}‘…äÁ}Á•É•¹Ðˆ°€‰±•…È¡…¹”™É½´…ä€Àˆ°€ˆ”ˆ¤°4(€€€€€€€€ ‰9%I}ÙÍ}‘…äÁ}Á•É•¹Ðˆ°€‰9%H¡…¹”™É½´…ä€Àˆ°€ˆ”ˆ¤°4(€€€€€€€€ ‰á|ØàÁ¹µ}±•…É}¹½É´ˆ°€ˆØàÀ¹´€¼±•…Èˆ°€‰É…Ñ¥¼ˆ¤°4(€€€€€€€€ ‰Ý•¥¡Ñ}±½ÍÍ}Á•É•¹Ðˆ°€‰]•¥¡Ð±½ÍÌ™É½´…ä€Àˆ°€ˆ”ˆ¤°4(€€€t4(4(€€€™½ÈÍ…µÁ±•}¥°Í…µÁ±”¥¸ÍÕµµ…Éä¹É½ÕÁ‰ä ‰Í…µÁ±•}¥ˆ¤è4(€€€€€€€Í…µÁ±”€ôÍ…µÁ±”¹Í½ÉÑ}Ù…±Õ•Ì ‰ÍÑ½É…•}‘…äˆ¤4(€€€€€€€™½È…á¥Ì°€¡½±Õµ¸°Ñ¥Ñ±”°Õ¹¥Ð¤¥¸é¥À¡…á•Ì¹™±…Ð°µ•ÑÉ¥Ì¤è4(€€€€€€€€€€€¥˜½±Õµ¸¹½Ð¥¸Í…µÁ±”¹½±Õµ¹Ìè4(€€€€€€€€€€€€€€€½¹Ñ¥¹Õ”4(€€€€€€€€€€€…á¥Ì¹Á±½Ð 4(€€€€€€€€€€€€€€€Í…µÁ±•l‰ÍÑ½É…•}‘…ä‰t°4(€€€€€€€€€€€€€€€Í…µÁ±•m½±Õµ¹t°4(€€€€€€€€€€€€€€€µ…É­•Èô‰¼ˆ°4(€€€€€€€€€€€€€€€±…‰•°õÍ…µÁ±•}¥°4(€€€€€€€€€€€€¤4(€€€€€€€€€€€…á¥Ì¹Í•Ñ}Ñ¥Ñ±”¡Ñ¥Ñ±”¤4(€€€€€€€€€€€…á¥Ì¹Í•Ñ}á±…‰•° ‰MÑ½É…”‘…äˆ¤4(€€€€€€€€€€€…á¥Ì¹Í•Ñ}å±…‰•°¡Õ¹¥Ð¤4(€€€€€€€€€€€…á¥Ì¹É¥¡…±Á¡„ôÀ¸ÈÔ¤4(4(€€€™½È…á¥Ì°€¡½±Õµ¸°Ñ¥Ñ±”°Õ¹¥Ð¤¥¸é¥À¡…á•Ì¹™±…Ð°µ•ÑÉ¥Ì¤è4(€€€€€€€…á¥Ì¹Í•Ñ}Ñ¥Ñ±”¡Ñ¥Ñ±”¤4(€€€€€€€…á¥Ì¹Í•Ñ}á±…‰•° ‰MÑ½É…”‘…äˆ¤4(€€€€€€€…á¥Ì¹Í•Ñ}å±…‰•°¡Õ¹¥Ð¤4(€€€€€€€…á¥Ì¹É¥¡…±Á¡„ôÀ¸ÈÔ¤4(€€€€€€€¥˜½±Õµ¸¹½Ð¥¸ÍÕµµ…Éä¹½±Õµ¹Ì½È¹½ÐÁ¹Ñ½}¹Õµ•É¥Œ 4(€€€€€€€€€€€ÍÕµµ…Éåm½±Õµ¹t°•ÉÉ½ÉÌô‰½•É”ˆ4(€€€€€€€€¤¹¹½Ñ¹„ ¤¹…¹ä ¤è4(€€€€€€€€€€€…á¥Ì¹Ñ•áÐ 4(€€€€€€€€€€€€€€€€À¸Ô°4(€€€€€€€€€€€€€€€€À¸Ô°4(€€€€€€€€€€€€€€€€‰9½Ðµ•…ÍÕÉ•‘q¹Á±…•¡½±‘•ÈÙ…±Õ•Ì•á±Õ‘•ˆ°4(€€€€€€€€€€€€€€€¡„ô‰•¹Ñ•Èˆ°4(€€€€€€€€€€€€€€€Ù„ô‰•¹Ñ•Èˆ°4(€€€€€€€€€€€€€€€ÑÉ…¹Í™½É´õ…á¥Ì¹ÑÉ…¹Íá•Ì°4(€€€€€€€€€€€€€€€½±½Èô‰‘¥µÉ…äˆ°4(€€€€€€€€€€€€¤4(4(€€€™½È…á¥Ì¥¸…á•Ì¹™±…Ðè4(€€€€€€€¥˜±•¸¡ÍÑ½É…•}‘…åÌ¤€ôô€Äè4(€€€€€€€€€€€…á¥Ì¹Í•Ñ}á±¥´¡ÍÑ½É…•}‘…åÍlÁt€´€À¸Ô°ÍÑ½É…•}‘…åÍlÁt€¬€À¸Ô¤4(€€€€€€€€€€€…á¥Ì¹Í•Ñ}áÑ¥­Ì¡ÍÑ½É…•}‘…åÌ¤4(€€€€€€€•±Í”è4(€€€€€€€€€€€…á¥Ì¹á…á¥Ì¹Í•Ñ}µ…©½É}±½…Ñ½È¡5…á91½…Ñ½È¡¥¹Ñ••ÈõQÉÕ”¤¤4(€€€€€€€¡…¹‘±•Ì°|€ô…á¥Ì¹•Ñ}±••¹‘}¡…¹‘±•Í}±…‰•±Ì ¤4(€€€€€€€¥˜¡…¹‘±•Ìè4(€€€€€€€€€€€…á¥Ì¹±••¹ ¤4(€€€‘…Ñ•Ì€ôÍ½ÉÑ•¡Ù…±Õ”™½ÈÙ…±Õ”¥¸ÍÕµµ…Éål‰½±±•Ñ¥½¹}‘…Ñ”‰t¹‘É½Á¹„ ¤¹Õ¹¥ÅÕ” ¤¥˜Ù…±Õ”¤4(€€€‘…Ñ•}Ñ•áÐ€ô€ˆÑ¼€ˆ¹©½¥¸¡‘…Ñ•Ì¤¥˜‘…Ñ•Ì•±Í”€‰‘…Ñ”Õ¹…Ù…¥±…‰±”ˆ4(€€€™¥ÕÉ”¹ÍÕÁÑ¥Ñ±”¡˜‰9•Ñ…É¥¹”™É•Í¡¹•ÍÌÑÉ…©•Ñ½É¥•Ì€¡í‘…Ñ•}Ñ•áÑô¤ˆ¤4(€€€Á…Ñ ¹Á…É•¹Ð¹µ­‘¥È¡Á…É•¹ÑÌõQÉÕ”°•á¥ÍÑ}½¬õQÉÕ”¤4(€€€™¥ÕÉ”¹Í…Ù•™¥œ¡Á…Ñ °‘Á¤ôÄàÀ¤4(€€€Á±Ð¹±½Í”¡™¥ÕÉ”¤4(4(4)‘•˜Í…Ù•}Á½Í¥Ñ¥½¹}Á±½Ð¡Á½Í¥Ñ¥½¹}ÍÕµµ…ÉäèÁ¹…Ñ…É…µ”°Á…Ñ èA…Ñ ¤€´ø9½¹”è4(€€€€ˆˆ‹šbûž’ëš¾?’â«šÂÓšzs–r£š¾?’â–’¥@Äµ@Óžj–:–ž/’ê»–ê›–J3–öK’â–2[–'¢ÂÇš2š‚Žˆˆˆ4(€€€Í…µÁ±•}¥‘Ì€ô±¥ÍÐ¡Á½Í¥Ñ¥½¹}ÍÕµµ…Éål‰Í…µÁ±•}¥‰t¹‘É½Á}‘ÕÁ±¥…Ñ•Ì ¤¤4(€€€™¥ÕÉ”°…á•Ì€ôÁ±Ð¹ÍÕ‰Á±½ÑÌ 4(€€€€€€€±•¸¡Í…µÁ±•}¥‘Ì¤°4(€€€€€€€€È°4(€€€€€€€™¥Í¥é”ô ÄÌ°µ…à Ð°€Ì¸Ø€¨±•¸¡Í…µÁ±•}¥‘Ì¤¤¤°4(€€€€€€€ÍÅÕ••é”õ…±Í”°4(€€€€€€€½¹ÍÑÉ…¥¹•‘}±…å½ÕÐõQÉÕ”°4(€€€€¤4(€€€™½ÈÉ½Ý}¥¹‘•à°Í…µÁ±•}¥¥¸•¹Õµ•É…Ñ”¡Í…µÁ±•}¥‘Ì¤è4(€€€€€€€Í…µÁ±”€ôÁ½Í¥Ñ¥½¹}ÍÕµµ…Éä¹±½mÁ½Í¥Ñ¥½¹}ÍÕµµ…Éål‰Í…µÁ±•}¥‰t€ôôÍ…µÁ±•}¥‘t4(€€€€€€€™½ÈÍÑ½É…•}‘…ä°‘…å}‘…Ñ„¥¸Í…µÁ±”¹É½ÕÁ‰ä ‰ÍÑ½É…•}‘…äˆ¤è4(€€€€€€€€€€€‘…å}‘…Ñ„€ô‘…å}‘…Ñ„¹Í½ÉÑ}Ù…±Õ•Ì ‰Á½Í¥Ñ¥½¸ˆ°­•äõ±…µ‰‘„ÌèÌ¹…ÍÑåÁ”¡¥¹Ð¤¤4(€€€€€€€€€€€™¥ÉÍÑ}Ñ¥µ”€ôÁ¹Ñ½}‘…Ñ•Ñ¥µ” 4(€€€€€€€€€€€€€€€‘…å}‘…Ñ…l‰½±±•Ñ¥½¹}ÍÑ…ÉÑ•‘}…Ð‰t¹¥±½lÁt°•ÉÉ½ÉÌô‰½•É”ˆ4(€€€€€€€€€€€€¤4(€€€€€€€€€€€Ñ¥µ•}±…‰•°€ô€ 4(€€€€€€€€€€€€€€€™¥ÉÍÑ}Ñ¥µ”¹ÍÑÉ™Ñ¥µ” ˆ•´´•€• è•4ˆ¤4(€€€€€€€€€€€€€€€¥˜Á¹¹½Ñ¹„¡™¥ÉÍÑ}Ñ¥µ”¤4(€€€€€€€€€€€€€€€•±Í”€‰Ñ¥µ”Õ¹…Ù…¥±…‰±”ˆ4(€€€€€€€€€€€€¤4(€€€€€€€€€€€±…‰•°€ô˜‰…äí¥¹Ð¡ÍÑ½É…•}‘…ä¥ôƒ
ÜíÑ¥µ•}±…‰•±ôˆ4(€€€€€€€€€€€Á½Í¥Ñ¥½¹Ì€ôm˜‰AíÙ…±Õ•ôˆ™½ÈÙ…±Õ”¥¸‘…å}‘…Ñ…l‰Á½Í¥Ñ¥½¸‰ut4(€€€€€€€€€€€…á•ÍmÉ½Ý}¥¹‘•à°€Át¹Á±½Ð 4(€€€€€€€€€€€€€€€Á½Í¥Ñ¥½¹Ì°‘…å}‘…Ñ…l‰±•…É}µ•…¸‰t°µ…É­•Èô‰¼ˆ°±…‰•°õ±…‰•°4(€€€€€€€€€€€€¤4(€€€€€€€€€€€…á•ÍmÉ½Ý}¥¹‘•à°€Åt¹Á±½Ð 4(€€€€€€€€€€€€€€€Á½Í¥Ñ¥½¹Ì°4(€€€€€€€€€€€€€€€‘…å}‘…Ñ…l‰á|ØàÁ¹µ}±•…É}¹½É´‰t°4(€€€€€€€€€€€€€€€µ…É­•Èô‰¼ˆ°4(€€€€€€€€€€€€€€€±…‰•°õ±…‰•°°4(€€€€€€€€€€€€¤4(€€€€€€€…á•ÍmÉ½Ý}¥¹‘•à°€Át¹Í•Ñ}Ñ¥Ñ±”¡˜‰íÍ…µÁ±•}¥‘ôè½ÉÉ•Ñ•±•…È‰äÁ½Í¥Ñ¥½¸ˆ¤4(€€€€€€€…á•ÍmÉ½Ý}¥¹‘•à°€Át¹Í•Ñ}å±…‰•° ‰1¥Ð€´…µ‰¥•¹Ð€¡½Õ¹Ð¤ˆ¤4(€€€€€€€…á•ÍmÉ½Ý}¥¹‘•à°€Åt¹Í•Ñ}Ñ¥Ñ±”¡˜‰íÍ…µÁ±•}¥‘ôè€ØàÀ¹´€¼±•…È‰äÁ½Í¥Ñ¥½¸ˆ¤4(€€€€€€€…á•ÍmÉ½Ý}¥¹‘•à°€Åt¹Í•Ñ}å±…‰•° ‰É…Ñ¥¼ˆ¤4(€€€€€€€™½È…á¥Ì¥¸…á•ÍmÉ½Ý}¥¹‘•átè4(€€€€€€€€€€€…á¥Ì¹Í•Ñ}á±…‰•° ‰5…É­•™ÉÕ¥ÐÁ½Í¥Ñ¥½¸ˆ¤4(€€€€€€€€€€€…á¥Ì¹É¥¡…±Á¡„ôÀ¸ÈÔ¤4(€€€€€€€€€€€…á¥Ì¹±••¹¡™½¹ÑÍ¥é”ôà¤4(€€€™¥ÕÉ”¹ÍÕÁÑ¥Ñ±” ‰É½ÍÌµ‘…ä½µÁ…É¥Í½¸‰ä™ÉÕ¥Ð°‘…Ñ”½Ñ¥µ”…¹@Äµ@Ðˆ¤4(€€€Á…Ñ ¹Á…É•¹Ð¹µ­‘¥È¡Á…É•¹ÑÌõQÉÕ”°•á¥ÍÑ}½¬õQÉÕ”¤4(€€€™¥ÕÉ”¹Í…Ù•™¥œ¡Á…Ñ °‘Á¤ôÄàÀ¤4(€€€Á±Ð¹±½Í”¡™¥ÕÉ”¤4(4(4)‘•˜ÉÕ¸¡…ÉÌè…ÉÁ…ÉÍ”¹9…µ•ÍÁ…”¤€´ø¥¹Ðè4(€€€É…Ý}‘¥È€ôA…Ñ ¡…ÉÌ¹É…Ý}‘¥È¤4(€€€¥˜¹½ÐÉ…Ý}‘¥È¹¥Í}…‰Í½±ÕÑ” ¤è4(€€€€€€€É…Ý}‘¥È€ôAI=)Q}I==P€¼É…Ý}‘¥È4(€€€¥˜…ÉÌ¹™É…µ•Í}Á•É}Á½Í¥Ñ¥½¸€ðô€Àè4(€€€€€€€É…¥Í”Y…±Õ•ÉÉ½È ˆ´µ™É…µ•ÌµÁ•ÈµÁ½Í¥Ñ¥½¸ƒ–þ¦†ï–’Ÿ’ê8Àˆ¤4(4(€€€…¹‘¥‘…Ñ•Ì°Í½ÕÉ•}™É…µ•Ì°Í­¥ÁÁ•€ô‘¥Í½Ù•É}…¹‘¥‘…Ñ•Ì 4(€€€€€€€É…Ý}‘¥È°4(€€€€€€€…ÉÌ¹™ÉÕ¥Ñ}ÑåÁ”°4(€€€€€€€…ÉÌ¹™É…µ•Í}Á•É}Á½Í¥Ñ¥½¸°4(€€€€¤4(€€€Í•±•Ñ¥½¸€ôÍ•±•Ñ}‰•ÍÑ}…¹‘¥‘…Ñ•Ì¡…¹‘¥‘…Ñ•Ì¤4(€€€Í•±•Ñ¥½¹}½Ù•ÉÉ¥‘•Í}Á…Ñ €ôA…Ñ ¡…ÉÌ¹Í•±•Ñ¥½¹}½Ù•ÉÉ¥‘•Ì¤4(€€€¥˜¹½ÐÍ•±•Ñ¥½¹}½Ù•ÉÉ¥‘•Í}Á…Ñ ¹¥Í}…‰Í½±ÕÑ” ¤è4(€€€€€€€Í•±•Ñ¥½¹}½Ù•ÉÉ¥‘•Í}Á…Ñ €ôAI=)Q}I==P€¼Í•±•Ñ¥½¹}½Ù•ÉÉ¥‘•Í}Á…Ñ 4(€€€Í•±•Ñ¥½¸€ô…ÁÁ±å}Í•±•Ñ¥½¹}½Ù•ÉÉ¥‘•Ì¡Í•±•Ñ¥½¸°Í•±•Ñ¥½¹}½Ù•ÉÉ¥‘•Í}Á…Ñ ¤4(€€€ÕÉ…Ñ•€ô‰Õ¥±‘}ÕÉ…Ñ•‘}™É…µ•Ì 4(€€€€€€€Í•±•Ñ¥½¸°4(€€€€€€€Í½ÕÉ•}™É…µ•Ì°4(€€€€€€€…ÉÌ¹™É…µ•Í}Á•É}Á½Í¥Ñ¥½¸°4(€€€€¤4(€€€½ÉÉ•Ñ¥½¹Í}Á…Ñ €ôA…Ñ ¡…ÉÌ¹µ•Ñ…‘…Ñ…}½ÉÉ•Ñ¥½¹Ì¤4(€€€¥˜¹½Ð½ÉÉ•Ñ¥½¹Í}Á…Ñ ¹¥Í}…‰Í½±ÕÑ” ¤è4(€€€€€€€½ÉÉ•Ñ¥½¹Í}Á…Ñ €ôAI=)Q}I==P€¼½ÉÉ•Ñ¥½¹Í}Á…Ñ 4(€€€ÕÉ…Ñ•°½ÉÉ•Ñ¥½¹}…Õ‘¥Ð€ô…ÁÁ±å}µ•Ñ…‘…Ñ…}½ÉÉ•Ñ¥½¹Ì 4(€€€€€€€ÕÉ…Ñ•°½ÉÉ•Ñ¥½¹Í}Á…Ñ 4(€€€€¤4(€€€¥˜…ÉÌ¹Ý•¥¡Ñ}Õ¹…Ù…¥±…‰±”è4(€€€€€€€ÕÉ…Ñ•‘l‰Ý•¥¡Ñ}œ‰t€ô¹À¹¹…¸4(€€€½µÁ±•Ñ•¹•ÍÌ€ô‰Õ¥±‘}½µÁ±•Ñ•¹•ÍÍ}É•Á½ÉÐ¡Í•±•Ñ¥½¸°…ÉÌ¹Á½Í¥Ñ¥½¹Ì¤4(€€€ÍÕµµ…Éä€ô‰Õ¥±‘}‘…¥±å}ÍÕµµ…Éä¡ÕÉ…Ñ•¤4(€€€Á½Í¥Ñ¥½¹}ÍÕµµ…Éä€ô‰Õ¥±‘}Á½Í¥Ñ¥½¹}ÍÕµµ…Éä¡ÕÉ…Ñ•¤4(€€€‘…¥±å}µ½‘•±}Í…µÁ±•Ì€ô‰Õ¥±‘}‘…¥±å}µ½‘•±}Í…µÁ±•Ì¡Á½Í¥Ñ¥½¹}ÍÕµµ…Éä¤4(€€€‘¥ÍÑ…¹•}É•Á½ÉÐ€ô‰Õ¥±‘}‘¥ÍÑ…¹•}½µÁ•¹Í…Ñ¥½¹}É•Á½ÉÐ¡Á½Í¥Ñ¥½¹}ÍÕµµ…Éä¤4(4(€€€ÁÉ•™¥à€ô…ÉÌ¹½ÕÑÁÕÑ}ÁÉ•™¥à¹É•Á±…” ˆ¼ˆ°€‰|ˆ¤¹É•Á±…” ‰qpˆ°€‰|ˆ¤4(€€€AI=MM}%H¹µ­‘¥È¡Á…É•¹ÑÌõQÉÕ”°•á¥ÍÑ}½¬õQÉÕ”¤4(€€€%UI}%H¹µ­‘¥È¡Á…É•¹ÑÌõQÉÕ”°•á¥ÍÑ}½¬õQÉÕ”¤4(€€€Í•±•Ñ¥½¹}Á…Ñ €ôAI=MM}%H€¼˜‰íÁÉ•™¥áõ}™¥±•}Í•±•Ñ¥½¸¹ÍØˆ4(€€€ÕÉ…Ñ•‘}Á…Ñ €ôAI=MM}%H€¼˜‰íÁÉ•™¥áõ}ÕÉ…Ñ•‘}™É…µ•Ì¹ÍØˆ4(€€€½µÁ±•Ñ•¹•ÍÍ}Á…Ñ €ôAI=MM}%H€¼˜‰íÁÉ•™¥áõ}½µÁ±•Ñ•¹•ÍÌ¹ÍØˆ4(€€€ÍÕµµ…Éå}Á…Ñ €ôAI=MM}%H€¼˜‰íÁÉ•™¥áõ}‘…¥±å}ÍÕµµ…Éä¹ÍØˆ4(€€€Á½Í¥Ñ¥½¹}ÍÕµµ…Éå}Á…Ñ €ôAI=MM}%H€¼˜‰íÁÉ•™¥áõ}Á½Í¥Ñ¥½¹}ÍÕµµ…Éä¹ÍØˆ4(€€€µ½‘•±}Í…µÁ±•Í}Á…Ñ €ôAI=MM}%H€¼˜‰íÁÉ•™¥áõ}‘…¥±å}µ½‘•±}Í…µÁ±•Ì¹ÍØˆ4(€€€‘¥ÍÑ…¹•}É•Á½ÉÑ}Á…Ñ €ôAI=MM}%H€¼˜‰íÁÉ•™¥áõ}‘¥ÍÑ…¹•}½µÁ•¹Í…Ñ¥½¸¹ÍØˆ4(€€€½ÉÉ•Ñ¥½¹}…Õ‘¥Ñ}Á…Ñ €ôAI=MM}%H€¼˜‰íÁÉ•™¥áõ}µ•Ñ…‘…Ñ…}½ÉÉ•Ñ¥½¹Í}…ÁÁ±¥•¹ÍØˆ4(€€€‰…Í•±¥¹•}Á…Ñ €ô%UI}%H€¼˜‰íÁÉ•™¥áõ}‘…äÁ}‰…Í•±¥¹”¹Á¹œˆ4(€€€ÑÉ…©•Ñ½Éå}Á…Ñ €ô%UI}%H€¼˜‰íÁÉ•™¥áõ}ÑÉ…©•Ñ½É¥•Ì¹Á¹œˆ4(€€€Á½Í¥Ñ¥½¹}Á…Ñ €ô%UI}%H€¼˜‰íÁÉ•™¥áõ}Á½Í¥Ñ¥½¹Í}‰å}Ñ¥µ”¹Á¹œˆ4(4(€€€Í•±•Ñ¥½¸¹Ñ½}ÍØ¡Í•±•Ñ¥½¹}Á…Ñ °¥¹‘•àõ…±Í”°•¹½‘¥¹œô‰ÕÑ˜´àµÍ¥œˆ¤4(€€€ÕÉ…Ñ•¹Ñ½}ÍØ¡ÕÉ…Ñ•‘}Á…Ñ °¥¹‘•àõ…±Í”°•¹½‘¥¹œô‰ÕÑ˜´àµÍ¥œˆ¤4(€€€½µÁ±•Ñ•¹•ÍÌ¹Ñ½}ÍØ¡½µÁ±•Ñ•¹•ÍÍ}Á…Ñ °¥¹‘•àõ…±Í”°•¹½‘¥¹œô‰ÕÑ˜´àµÍ¥œˆ¤4(€€€ÍÕµµ…Éä¹Ñ½}ÍØ¡ÍÕµµ…Éå}Á…Ñ °¥¹‘•àõ…±Í”°•¹½‘¥¹œô‰ÕÑ˜´àµÍ¥œˆ¤4(€€€Á½Í¥Ñ¥½¹}ÍÕµµ…Éä¹Ñ½}ÍØ¡Á½Í¥Ñ¥½¹}ÍÕµµ…Éå}Á…Ñ °¥¹‘•àõ…±Í”°•¹½‘¥¹œô‰ÕÑ˜´àµÍ¥œˆ¤4(€€€‘…¥±å}µ½‘•±}Í…µÁ±•Ì¹Ñ½}ÍØ¡µ½‘•±}Í…µÁ±•Í}Á…Ñ °¥¹‘•àõ…±Í”°•¹½‘¥¹œô‰ÕÑ˜´àµÍ¥œˆ¤4(€€€‘¥ÍÑ…¹•}É•Á½ÉÐ¹Ñ½}ÍØ¡‘¥ÍÑ…¹•}É•Á½ÉÑ}Á…Ñ °¥¹‘•àõ…±Í”°•¹½‘¥¹œô‰ÕÑ˜´àµÍ¥œˆ¤4(€€€½ÉÉ•Ñ¥½¹}…Õ‘¥Ð¹Ñ½}ÍØ¡½ÉÉ•Ñ¥½¹}…Õ‘¥Ñ}Á…Ñ °¥¹‘•àõ…±Í”°•¹½‘¥¹œô‰ÕÑ˜´àµÍ¥œˆ¤4(€€€Í…Ù•}‘…äÁ}‰…Í•±¥¹•}Á±½Ð¡ÍÕµµ…Éä°‰…Í•±¥¹•}Á…Ñ ¤4(€€€Í…Ù•}ÑÉ…©•Ñ½Éå}Á±½Ð¡ÍÕµµ…Éä°ÑÉ…©•Ñ½Éå}Á…Ñ ¤4(€€€Í…Ù•}Á½Í¥Ñ¥½¹}Á±½Ð¡Á½Í¥Ñ¥½¹}ÍÕµµ…Éä°Á½Í¥Ñ¥½¹}Á…Ñ ¤4(4(€€€Í•±•Ñ•€ôÍ•±•Ñ¥½¸¹±½mÍ•±•Ñ¥½¹l‰Í•±•Ñ•‰t€ôô€Åt4(€€€µ¥ÍÍ¥¹œ€ô½µÁ±•Ñ•¹•ÍÌ¹±½m½µÁ±•Ñ•¹•ÍÍl‰ÍÑ…ÑÕÌ‰t€ôô€‰µ¥ÍÍ¥¹œ‰t4(€€€¥¹½µÁ±•Ñ”€ô½µÁ±•Ñ•¹•ÍÌ¹±½m½µÁ±•Ñ•¹•ÍÍl‰ÍÑ…ÑÕÌ‰t€ôô€‰¥¹½µÁ±•Ñ”‰t4(€€€Õ¹ÍÑ…‰±”€ôÍ•±•Ñ•¹±½l4(€€€€€€€Í•±•Ñ•‘l‰µ•‘¥…¹}Ù}Á•É•¹Ð‰t€ø…ÉÌ¹Ù}Ý…É¹¥¹}Á•É•¹Ð4(€€€t4(€€€ÍÑ…Ñ•Ì€ôÍ½ÉÑ• 4(€€€€€€€Í•Ð¡ÕÉ…Ñ•‘l‰™É•Í¡¹•ÍÍ}ÍÑ…Ñ”‰t¹…ÍÑåÁ”¡ÍÑÈ¤¤°4(€€€€€€€­•äõ±…µ‰‘„Ù…±Õ”èMQQ}=IH¹•Ð¡Ù…±Õ”°€ää¤°4(€€€€¤4(4(€€€ÁÉ¥¹Ð ˆôôôƒšÊçš†šZÃ¦Ês–ê›¢þ{žî·–º{¦ª3šŽš~”€ôôôˆ¤4(€€€ÁÉ¥¹Ð¡˜‹–g¦'–:–ž/šZ’îØèí±•¸¡Í•±•Ñ¥½¸¥ôƒ’â¨ˆ¤4(€€€ÁÉ¥¹Ð¡˜‹¦'žR£šZ’îØèí±•¸¡Í•±•Ñ•¥ôƒ’â¨ˆ¤4(€€€ÁÉ¥¹Ð¡˜‹š:K¦f“¦7–’4¿¦7šÖ/šZ’îØèí±•¸¡Í•±•Ñ¥½¸¤€´±•¸¡Í•±•Ñ•¥ôƒ’â¨ˆ¤4(€€€ÁÉ¥¹Ð¡˜‹š¶–ò?šr'šV#–âœèí±•¸¡ÕÉ…Ñ•¥ôƒšv„ˆ¤4(€€€ÁÉ¥¹Ð¡˜‹ž.³ž®/šÊçš†èíÕÉ…Ñ•‘lÍ…µÁ±•}¥t¹¹Õ¹¥ÅÕ” ¥ôƒ’â¨ˆ¤4(€€€‘…åÌ€ôÍ½ÉÑ•¡¥¹Ð¡Ù…±Õ”¤™½ÈÙ…±Õ”¥¸ÕÉ…Ñ•‘l‰ÍÑ½É…•}‘…ä‰t¹Õ¹¥ÅÕ” ¤¤4(€€€ÁÉ¥¹Ð¡˜‹–
£–¶c–’§šVÀèí‘…åÍôˆ¤4(€€€ÁÉ¥¹Ð¡˜‹–ÞËšr'ž*Ûšèìœ°€œ¹©½¥¸¡ÍÑ…Ñ•Ì¥ôˆ¤4(€€€ÁÉ¥¹Ð¡˜‹–ÞË–êSžR£–šVÃš6»šnÓš¶Œèí±•¸¡½ÉÉ•Ñ¥½¹}…Õ‘¥Ð¥ôƒ¦†äˆ¤4(4(€€€¥˜Í­¥ÁÁ•è4(€€€€€€€ÁÉ¥¹Ð ‰q¹o¢ÞÏ¢þšZ’îÙtˆ¤4(€€€€€€€™½Èµ•ÍÍ…”¥¸Í­¥ÁÁ•è4(€€€€€€€€€€€ÁÉ¥¹Ð¡˜ˆ´íµ•ÍÍ…•ôˆ¤4(€€€¥˜¹½Ðµ¥ÍÍ¥¹œ¹•µÁÑäè4(€€€€€€€ÁÉ¥¹Ð¡˜‰q¹ožòë–ÂGšVÃš6¹tí±•¸¡µ¥ÍÍ¥¹œ¥ôƒ’â«š‚ß–N·–’§šVÀ·’ö7žö»žî–B ˆ¤4(€€€€€€€ÁÉ¥¹Ð¡µ¥ÍÍ¥¹ml‰Í…µÁ±•}¥ˆ°€‰ÍÑ½É…•}‘…äˆ°€‰Á½Í¥Ñ¥½¸‰ut¹Ñ½}ÍÑÉ¥¹œ¡¥¹‘•àõ…±Í”¤¤4(€€€¥˜¹½Ð¥¹½µÁ±•Ñ”¹•µÁÑäè4(€€€€€€€ÁÉ¥¹Ð¡˜‰q¹o–âŸšVÃ’â7¢ÚÍtí±•¸¡¥¹½µÁ±•Ñ”¥ôƒžîˆ¤4(€€€¥˜¹½ÐÕ¹ÍÑ…‰±”¹•µÁÑäè4(€€€€€€€ÁÉ¥¹Ð¡˜‰q¹ož¢Ï–ºkšŸ¢¶›–F)tí±•¸¡Õ¹ÍÑ…‰±”¥ôƒžî[’â·’ö7šVÃ¢Ú¢þ¦b#–ðˆ¤4(€€€€€€€ÁÉ¥¹Ð 4(€€€€€€€€€€€Õ¹ÍÑ…‰±•l4(€€€€€€€€€€€€€€€l‰Í…µÁ±•}¥ˆ°€‰ÍÑ½É…•}‘…äˆ°€‰Á½Í¥Ñ¥½¸ˆ°€‰µ•‘¥…¹}Ù}Á•É•¹Ðˆ°€‰Í½ÕÉ•}™¥±”‰t4(€€€€€€€€€€€t¹Ñ½}ÍÑÉ¥¹œ¡¥¹‘•àõ…±Í”¤4(€€€€€€€€¤4(€€€µ½‘•±}É•…‘ä°µ½‘•±}µ•ÍÍ…”€ô…ÍÍ•ÍÍ}µ½‘•±}É•…‘¥¹•ÍÌ¡‘…¥±å}µ½‘•±}Í…µÁ±•Ì¤4(€€€ÍÑ…ÑÕÌ€ô€‹–>¿¢º·žîˆ¥˜µ½‘•±}É•…‘ä•±Í”€‹šj’â7–>¿¢º·žîˆ4(€€€ÁÉ¥¹Ð¡˜‰q¹oš¢‡–z/ž*Ûš¾òiíÍÑ…ÑÕÍõtíµ½‘•±}µ•ÍÍ…•ôˆ¤4(4(€€€ÁÉ¥¹Ð ‰q»¢úO–ëšZ’îØèˆ¤4(€€€™½ÈÁ…Ñ ¥¸€ 4(€€€€€€€Í•±•Ñ¥½¹}Á…Ñ °4(€€€€€€€ÕÉ…Ñ•‘}Á…Ñ °4(€€€€€€€½µÁ±•Ñ•¹•ÍÍ}Á…Ñ °4(€€€€€€€ÍÕµµ…Éå}Á…Ñ °4(€€€€€€€Á½Í¥Ñ¥½¹}ÍÕµµ…Éå}Á…Ñ °4(€€€€€€€µ½‘•±}Í…µÁ±•Í}Á…Ñ °4(€€€€€€€‘¥ÍÑ…¹•}É•Á½ÉÑ}Á…Ñ °4(€€€€€€€½ÉÉ•Ñ¥½¹}…Õ‘¥Ñ}Á…Ñ °4(€€€€€€€‰…Í•±¥¹•}Á…Ñ °4(€€€€€€€ÑÉ…©•Ñ½Éå}Á…Ñ °4(€€€€€€€Á½Í¥Ñ¥½¹}Á…Ñ °4(€€€€¤è4(€€€€€€€ÁÉ¥¹Ð¡˜ˆ´íÁ…Ñ¡ôˆ¤4(€€€É•ÑÕÉ¸€À4(4(4)‘•˜µ…¥¸ ¤€´ø¥¹Ðè4(€€€ÑÉäè4(€€€€€€€É•ÑÕÉ¸ÉÕ¸¡Á…ÉÍ•}…ÉÌ ¤¤4(€€€•á•ÁÐ€¡¥±•9½Ñ½Õ¹‘ÉÉ½È°Y…±Õ•ÉÉ½È°Á¹•ÉÉ½ÉÌ¹A…ÉÍ•ÉÉÉ½È¤…Ì•ÉÉ½Èè4(€€€€€€€ÁÉ¥¹Ð¡˜‰o¦Rg¢¾½tí•ÉÉ½Éôˆ¤4(€€€€€€€É•ÑÕÉ¸€Ä4(4(4)¥˜}}¹…µ•}|€ôô€‰}}µ…¥¹}|ˆè4(€€€É…¥Í”MåÍÑ•µá¥Ð¡µ…¥¸ ¤¤4(