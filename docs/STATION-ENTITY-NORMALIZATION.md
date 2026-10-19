# Station entity normalization

Schema version 11 separates the reusable station context into three
normalized entities:

- `operator_profile`: operator callsign and display name;
- `operating_location`: stable location key, grid square and DXCC hint;
- `dxcc_entity`: the numeric DXCC entity identity.

`station_profile` keeps its historical text and numeric columns for backward
compatibility and auditability, while also storing foreign-key references to
the normalized entities. Migration 11 creates the tables, adds the references
to existing databases, and backfills them from the legacy values. Saving a
profile upserts the entities and reuses an existing compatible row instead of
creating duplicates.

The automatic tests cover schema migration, legacy-column preservation,
shared-entity reuse, and DXCC identity persistence. Conflicts between station
profiles are still reported by the profile model and are never silently merged.
`StationProfileModel::awardAccountGroups()` now exposes a read-only grouping by
award account. Each group lists its profiles, a `compatible` flag and
deduplicated conflict reasons; only groups without conflicts are eligible for
aggregation by a caller. The model never merges rows automatically.

The tests also cover compatible profiles with non-overlapping validity periods
and incompatible profiles with different operators, locations and DXCC
entities. Real operator/location identity and external award-account policy
remain an operator decision; the normalized rows do not claim that two profiles
are interchangeable for awards without that review.
