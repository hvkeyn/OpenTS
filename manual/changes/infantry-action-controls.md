---
title: Give every infantry type its action controls up front
category: fix
release: 0.2.3
targets: []
credit: [OxFF]
---

A type whose artwork names no sequence kept a null action-control block, and the save
writer copied from it, so an autosave crashed. Every type now carries a full, zeroed set
from construction.
