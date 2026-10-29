## clm_ua — language-neutral contract

Heuristic browser and OS detection, version extraction, desktop/mobile/tablet classification, and crawler token check. Specific browser tokens (Edge, Opera, Samsung Internet) take priority over generic Chrome/Safari. Unknown results are named `Unknown`. This is not identity verification or a reliable security signal; User-Agent strings are spoofable.

### Cross-language expectations

This document specifies observable behavior, not syntax or compiler bindings. Keep input order, output values, error cases, and data ownership consistent across CLM language ports. For C signatures, see `include/clm_ua.h`.
