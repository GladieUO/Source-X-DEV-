# Core gains based on the existing radius

There is no new skill property, command or activation flag for this update.
The existing GAINRADIUS is the only switch:

- Positive radius: every eligible successful gain uses the fixed 75%-125% curve.
- Radius 0/unset on initial load: keep the previous flat advancement behavior.

The core compares difficulty with the player's current base skill. At one
radius below skill, the multiplier is 0.75; at matching skill it is 1.00; at
one radius above skill or harder it caps at 1.25. The existing radius check
still rejects challenges below the eligible range. Existing normalization
of difficulty to 1..1000 is retained.

Example: skill 60.0, radius 20.0, normal chance 10%, failure setting 50:

| Difficulty | Success debug chance | Failure debug chance |
| --- | ---: | ---: |
| 39 | Blocked | Blocked |
| 41 | 7.6% | 5.0% |
| 50 | 8.8% | 5.0% |
| 60 | 10.0% | 5.0% |
| 70 | 11.3% | 5.0% |
| 80+ | 12.5% | 5.0% |

The multiplier follows @SkillGain/@Gain adjustments and vetoes. ADV_RATE
still uses actual base skill, never an extrapolated adjusted-skill curve.
The result is capped at 100% probability. Normal caps, skill locks, safe
regions and custom script restrictions still apply.

Failures continue using SkillFailGainPercent of the unweighted chance,
including the previously implemented full-difficulty radius fix. The new
success weighting leaves the existing skill-decay chance, stat-gain path
and random-roll count unchanged. Integer rounding and the inclusive roll
comparison are retained.

Existing positive radii, including the crafting radius 20.0, automatically
use the calculation. No Taming or Lockpicking radius was added by this
update. Explicitly set GAINRADIUS=0 to disable it during a resource resync;
removing a line does not necessarily reset an already loaded resource.
Radius 100.0 permits the full 0-100 difficulty range, but being positive
it now also enables the weighting.

Validation: 421 native regression checks and 82,000 random-roll evaluations
cover the agreed curve, monotonicity, bounds, unchanged no-radius behavior,
unchanged failure gains, hooks, caps, aborts, safe regions, stat processing
and skill decay. Command:
python C:/sphere/scripts/.tools/check-skill-fail-gain.py

These checks compile the actual core function bodies in isolated fixtures.
No running server or live configuration has been changed. The rebuilt core
still needs in-game verification.
