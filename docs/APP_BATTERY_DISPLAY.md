# App change: robot and remote battery (for the elxie-ui team)

The robot firmware now sends both batteries in every `telemetry` message (every 200 ms while the app is connected over Bluetooth). Nothing needs to be requested.

```json
"battery": { "robot": 84, "robotV": 8.12, "remote": 80, "remoteV": 4.01 }
```

- `robot`: robot battery, 0..100 %. Always present.
- `remote`: the paired hand remote's battery, 0..100 %. **Left out** when the remote is off, out of range or not paired (nothing heard for 10 s).
- `robotV` / `remoteV`: voltages, for a details screen or support. Optional to show.

## What to change in elxie-ui

1. `types/protocol.ts`, telemetry payload:
   ```ts
   battery: {
     robot: number;
     robotV?: number;
     remote?: number;   // missing = remote not reporting
     remoteV?: number;
   };
   ```
2. `components/AppHeader.tsx`:
   - replace `const remoteBattery = 0;` with `const remoteBattery = telemetry?.battery?.remote;`
   - show the RC bar only when `remoteBattery !== undefined`; otherwise show it greyed with "–" (not 0 %, which looks like a dead battery)
   - un-comment the robot bar: `<BatteryBar label="Bot" pct={robotBattery} accent="green" />`
3. Optional: below 20 %, colour the bar red and show a "Charge the robot" / "Charge the remote" hint.

## Testing without the app

Over USB at 115200, send `battery?`. The robot answers:

```
BATTERY robot 8.12 V 84% remote 4.01 V 80%
```

(`remote none` when the remote is not reporting).
