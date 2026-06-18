# audit_wav_and_metadata.py
import csv, os, sys
from pathlib import Path
import soundfile as sf


def find_wav(acq: Path):
    cand = acq / "imp23absu_mic.wav"
    if cand.is_file(): return cand
    for p in acq.iterdir():
        if p.suffix.lower() == ".wav":
            return p
    return None

def main(root, frame_len, offset=0, wav_dir="wav"):
    root = Path(root)
    meta = root / "metadata.csv"
    if not meta.is_file():
        print(f"[ERR] metadata.csv not found under {root}")
        sys.exit(1)

    bad_missing, bad_short, ok = 0, 0, 0
    rows_out = []

    with meta.open("r", encoding="utf-8-sig") as f:
        rdr = csv.DictReader(f)
        for i, row in enumerate(rdr, 1):
            loc, acq, lab = row["location"], row["acqName"], row["label"]
            acq_dir = root / loc / acq
            wav = find_wav(acq_dir) if acq_dir.is_dir() else None

            if wav is None:
                bad_missing += 1
                rows_out.append([loc, acq, lab, "missing_wav", "", "", ""])
                print(f"[MISS] {acq_dir}")
                continue

            try:
                data, sr = sf.read(wav, dtype="int16", always_2d=True)
                n = data.shape[0]
                n_after = max(0, n - offset)
                ok_len = n_after >= frame_len
                if not ok_len:
                    bad_short += 1
                    reason = f"short: n={n}, after_offset={n_after} < frame_len={frame_len}"
                else:
                    ok += 1
                    reason = "ok"
                rows_out.append([loc, acq, lab, reason, str(wav), sr, n])
            except Exception as e:
                bad_missing += 1
                rows_out.append([loc, acq, lab, f"read_error:{e}", str(wav), "", ""])
                print(f"[ERR ] read {wav}: {e}")

    # 写报告
    out = root / "audit_report.csv"
    with out.open("w", newline="", encoding="utf-8-sig") as w:
        wr = csv.writer(w)
        wr.writerow(["location","acqName","label","status","wav_path","sr","n_samples"])
        wr.writerows(rows_out)

    print(f"\n[SUMMARY] ok={ok}, missing/err={bad_missing}, too_short={bad_short}")
    print(f"[REPORT ] {out}")

if __name__ == "__main__":
    # 用法示例：python audit_wav_and_metadata.py E:\SPEECH_DATA\20251029-command 188416 0
    # frame_len=46*4096=188416；offset(样本)按你实际传参
    root = sys.argv[1]
    frame_len = int(sys.argv[2])    # 例：46*4096=188416
    offset = int(sys.argv[3]) if len(sys.argv) > 3 else 0
    main(root, frame_len, offset)
