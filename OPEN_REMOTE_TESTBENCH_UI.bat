@echo off
rem Opens the remote testbench dashboard in Microsoft Edge (flash it first with REMOTE_TESTBENCH.bat).
start "" msedge "%~dp0tools\remote-testbench-ui\RemoteTestbench.html"
