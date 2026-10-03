---
title: Refuse a reinforcement whose team type carries no task force
category: fix
release: 0.2.3
targets: []
credit: [OxFF]
---

A team type read from a map can carry no task force or no script, and the reinforcement
action read through the null pointer. The action now refuses such a team instead of
crashing.
