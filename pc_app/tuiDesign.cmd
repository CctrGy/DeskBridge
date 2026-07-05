@echo off
mode con cols=120 lines=30
chcp 65001
cls



rem ╔╗╚╝║═
rem me parece que es el tamaño correcto de una ventana pordefecto de terminal de CMD
echo   DESKBRIDGE TUI MONITOR                           COM{x}   1209:DB01  {UID}  FW {-}  PROTOL: {-}   RTC {--:--:--}
echo ┌──[ RAW RECIVED ]────────────────────────────────────────────────────────────────────────────────┬──[ ERROR ]─────────┐
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │ FROM HERE UP LINES                                                                              │                    │
echo ├──[ INFO LINK ]──────────────────────────────────────────────────────────────────────────────────┤                    │
echo │                                                                                                 │                    │
echo ├──[ SHELL ]──────────────────────────────────────────────────────────────────────────────────────┤                    │
echo │ DB/>                                                                                            │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │ FROM HERE UP LINES │
echo │                                                                                                 ├──[ EVENT ]─────────┤
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │                    │
echo │                                                                                                 │ FROM HERE UP LINES │
echo └─────────────────────────────────────────────────────────────────────────────────────────────────┴────────────────────┘
echo              F5 Refresh all data    F8 Disconnect/Connect    F11 Full screen           F10 Help      F9 Exit  
set /p PAUSE=
