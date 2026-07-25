# Visible Backup-RAM Validation

Last validated: 2026-07-24

## Result

The Release host passed a visible, cross-process validation of the game's
native save and Data Load flow. The test used the supported USA ROM and the
pinned Beetle PCE core in an isolated working directory, so the normal
`portable-data/devils-crush.sav` was not read or modified.

The first process:

1. Started a new one-player game at the default speed.
2. Paused and opened the High Scores screen.
3. Opened the save screen, where all five slots initially displayed
   `NOT USE`.
4. Saved the current game into slot 1.
5. Exited cleanly so the host could atomically flush backup RAM.

The resulting `portable-data/devils-crush.sav` was exactly 2,048 bytes and
had SHA-256
`42def247b77a32ff8121c43291155049e5b472293c1cd61ea9b7b9244985d39c`.

A second Release-host process started in the same isolated directory. Its
Data Load screen displayed slot 1 as `00000000 03` while slots 2 through 5
still displayed `NOT USE`. Selecting slot 1 returned to the playfield with a
zero score and three balls, matching the saved game.

This validates the user-visible chain from the original game's save UI,
through host-side file persistence, into the original Data Load UI after a
complete process restart. The ROM-derived screenshots and save file remain
under ignored `out/` storage and are not distributable project artifacts.

## Repeating the Check

Use a fresh working directory containing no `portable-data` folder, then
launch the Release executable with an absolute ROM path:

```powershell
.\devils-crush.exe --rom "D:\path\to\Devil's Crush.pce"
```

Follow the save sequence described in the
[official USA manual](https://www.tg-16.com/contributors/bernie/Devil%27s_Crush/Devil%27s_Crush.pdf):
start a new game, pause, use Select to open High Scores, press Button I to
open the password/save screen, then press Button I on an unused slot. Exit
the host, confirm `portable-data/devils-crush.sav` is exactly 2,048 bytes,
restart from the same directory, choose Data Load, and load the saved slot.

For an automated byte-for-byte persistence check, use the
`--backup-ram-test` command documented in the repository README.
