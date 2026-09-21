# MyBot runtime bundle

This directory groups the three build inputs that make up the MyBot runtime:

- `mybot_sdk/` contains the read-only MyBot SDK snapshot and its ESP-IDF wrapper.
- `aosl/` contains the pinned AOSL component used by MyBot and RTSA.
- `agora_rtc/` contains the separately licensed Agora RTSA component.

The directories remain independent ESP-IDF components. The bundle only provides a common
location and does not merge their source trees or licenses. Update the exact revisions in
`VENDORED_SOURCES.md` whenever one of these inputs changes.
