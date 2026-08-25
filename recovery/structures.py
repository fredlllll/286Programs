"""Core data structures for the HDD recovery tool.

Every on-disk and in-memory concept gets its own dataclass here instead
of living as anonymous dict keys.  This gives autocomplete, catches
typos at import time, and makes the code self-documenting.

No imports from other project modules — this is the foundation layer.
"""

from __future__ import annotations

from dataclasses import dataclass, field

# ---- status codes (shared by the 286 C code and the Python reader) ----

ST_OK: int = 0x00               # read clean, data present
ST_ECC: int = 0x11              # read after ecc correction, data present
ST_HEADSKIP: int = 0xFE         # head masked out this pass, never attempted


def has_data(status: int) -> bool:
    """True when the descriptor's sector carries real data."""
    return status in (ST_OK, ST_ECC)


# ---- on-disk structures ----

@dataclass
class Descriptor:
    """One 6-byte entry in a descriptor block (lba + status + data index + crc8)."""
    lba: int
    status: int
    data_idx: int
    crc8: int = 0


@dataclass
class DescriptorBlock:
    """A parsed 512-byte descriptor block plus its following data sectors."""
    entries: list[Descriptor]
    datas: list[bytes]
    crc_ok: bool

    @property
    def first_lba(self) -> int:
        return self.entries[0].lba if self.entries else 0


# ---- validation / assembly results ----

@dataclass
class FloppyEval:
    """Validation result of one 1.44 MB dump image."""
    ok: bool = False
    reason: str = ''
    blocks: list[DescriptorBlock] = field(default_factory=list)
    desc_total: int = 0
    data_total: int = 0
    blocks_bad: int = 0


@dataclass
class DumpRecord:
    """One accepted dump disk: its filename and evaluation."""
    file: str
    ev: FloppyEval


@dataclass
class Overlap:
    """A file that overlapped with already-placed sectors."""
    file: str
    count: int


@dataclass
class StitchReport:
    """Everything the caller needs after stitching floppies together."""
    gaps: list[tuple[int, int]]
    chosen_files: list[str]
    err_codes: dict[int, int]
    headskipped_lbas: set[int]
    suspect_count: int
    coverage: int
    overlaps: list[Overlap]
    upgrades: int
    complete: bool
    total: int
    rejected: list[tuple[str, str]]
    records: list[DumpRecord]
