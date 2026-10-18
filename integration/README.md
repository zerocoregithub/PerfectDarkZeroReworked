# Integration inventories

The CSVs compare exported source files with locally available historical Git blob
references. They contain paths and hashes only, not upstream objects or artwork.
Different bytes can reflect line endings as well as code changes; this is not a
semantic diff or a tested cherry-pick series. Files absent from the reference can
include vendored dependencies that were formerly submodules, not just new code.
Omitted upstream files are not listed as deletions: this is a deliberately bounded
source export, not a complete upstream checkout. See the game integration guide
and SDK source manifest for component boundaries and dependency provenance.
