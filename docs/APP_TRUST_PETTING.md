# App change: trust level from petting (for the elxie-ui team)

The robot now reacts when it is petted at home (touch pad held more than 0.5 s while the robot is idle): blushing face, gentle LED animation and cute sounds. To make the trust level match what the child sees, count trust from the new telemetry field instead of `touch.event`.

## New telemetry (every 200 ms while connected)

```json
"pet": { "active": true, "seconds": 3 }
```

- `active`: `true` while the child is petting. Quick taps (the double tap that opens My Programs on the robot) are **not** petting.
- `seconds`: how long this petting has lasted.
- `touch.event` stays as before ("touched" on any touch), so nothing breaks before the app changes.

## What to change in elxie-ui

`context/BleContext.tsx`, the effect that raises trust:

```ts
// before: if (telemetry?.touch?.event === "touched") ...
const petting = telemetry?.pet?.active === true;
if (petting) {
  const now = Date.now();
  if (now - lastHoldTimeRef.current > 1000) {   // +1 per second of petting, as today
    lastHoldTimeRef.current = now;
    incrementTrustLevel(1);
  }
}
```

and add to the telemetry type in `types/protocol.ts`:

```ts
pet?: { active: boolean; seconds: number };
```

## Suggestions (optional)

- Trust is kept in the browser's localStorage today, so it is per phone/browser and starts at 50 again on a new device. Storing it with the account (like game progress) keeps it with the child.
- A small heart/trust "+1" animation in the app while `pet.active` is true makes the link between petting and trust visible.
