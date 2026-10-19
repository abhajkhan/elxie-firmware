@echo off
rem Opens the Elxie testbench dashboard (live sensor view) in Microsoft Edge.
rem Flash the testbench first with TESTBENCH_UI.bat or TESTBENCH.bat.
start "" msedge "%~dp0tools\testbench-ui\ElxieTestbench.html"
