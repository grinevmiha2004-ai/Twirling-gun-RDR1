# Twirl Gun: static findings in supplied Rampage.asi

Input SHA256: `3440e5681f47d35391749b78aca5c4680fa5962516ee17926753378be3f6a297`. The DLL was inspected as data, never loaded or executed. Addresses below are preferred VAs in this exact binary and are evidence references only; the mod does not call into Rampage or use its offsets.

## Menu-to-callback link

`Twirl Gun` string: file offset 0x1EBA68 / VA 0x1801ED068. Nearby description is `Holster gun with a twirl.` The menu construction uses callable vtable 0x1801DAF20. Its invoke method 0x180136CF0 jumps to 0x1800D30E0. That callback calls resource helpers 0x180033590 and 0x180033780.

## Observed recipe

Resource helpers request and wait for:

```text
REQUEST_ANIM_SET("holster_spin", 0)
HAS_ANIM_SET_LOADED("holster_spin")
REQUEST_ACTION_TREE("custom/holster_spin")
HAS_ACTION_TREE_LOADED("custom/holster_spin")
```

The callback verifies a drawn weapon, no mount/vehicle, and weapon enum <=7. It then performs:

```text
player = GET_LOCAL_SLOT()
SET_PLAYER_ENDLESS_READYMODE(player, 1)
SET_ANIM_SET_FOR_ACTOR(actor, "holster_spin", 1)
seq = TASK_SEQUENCE_OPEN()
TASK_ACTION_PERFORM(0, "holster_spin")
TASK_SEQUENCE_CLOSE()
TASK_SEQUENCE_PERFORM(actor, seq)
TASK_SEQUENCE_RELEASE(seq, 1)
while GET_NTH_TASK_STATUS(actor, 0) == 1: yield
SET_PLAYER_ENDLESS_READYMODE(GET_LOCAL_SLOT(), 0)
```

Key instruction references: set at 0x1800D3511–0x1800D3540; sequence open at 0x1800D359C–0x1800D35A2; action at 0x1800D35FC–0x1800D3618; close at 0x1800D3672–0x1800D3678; perform at 0x1800D36CF–0x1800D36F7; release at 0x1800D3751–0x1800D376B; status loop at 0x1800D37D4–0x1800D3810; ready-off at 0x1800D38CB–0x1800D38EB.

The imported native bridge is nativeInit at IAT VA 0x1801A1508, nativePush64 at 0x1801A1558, and nativeCall at 0x1801A1520. The encoded native identifiers decode using seed 0x4717358C, also visibly initialized at 0x1800FA678. Decoded resource hashes match the independent RDR native database (0x2988B3FC, 0x4FFF397D, 0xB3039DB7, 0xEEECD85E); the callback hashes resolve to the functions above.

## Differences in 0.2

The mod adds double-tap detection, revolver-only filtering, resource/draw/task timeouts, original-weapon redrawing when the first tap already holstered it, and F10 cancellation. Unlike the callback's unbounded status loop, every owned-state failure path releases ready mode. The mod resets its assigned set on termination; forced interruption also clears its own submitted task and resets the action tree. These additional cleanup operations are regression-tested with a fake game API but still require in-game validation.

This evidence establishes the trainer's invocation, not a successful run of the rebuilt mod. No trainer code or binary is bundled.
