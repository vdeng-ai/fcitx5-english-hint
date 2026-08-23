# Phase 0 manual verification

Target: Ubuntu 24.04 + Fcitx5 5.1.7 + fcitx5-rime 5.1.4 + rime-ice.

## Expected behavior

1. Start Fcitx5 with the addon installed.
2. Switch to Rime / rime-ice.
3. Type a phrase such as `tigao xiaolv`.
4. Every visible Rime candidate should gain a display-only suffix:

   ```text
   1. 提高效率 [TEST]
   2. 提高效益 [TEST]
   ```

5. Select a candidate with the number key or mouse.
6. The application must receive only the original Chinese text, for example
   `提高效率`, never `[TEST]`.

## Regression checks

- Preedit/pinyin does not gain `[TEST]`.
- Candidate labels (`1`, `2`, ...) do not gain `[TEST]`.
- Non-Rime input methods are unchanged.
- Candidate paging still works.
- Arrow-key candidate highlight still works.
- Mouse candidate selection still works.
- Final committed text is unchanged.
- Fcitx5 remains responsive if the addon is enabled.

## Useful diagnostics

```bash
fcitx5-diagnose | less
fcitx5 -rd 2>&1 | grep -i english-hint
```

Expected log line:

```text
fcitx5-english-hint Phase 0 loaded
```
