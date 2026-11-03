# Failure advancement (TLK branch)

Enable in the deployed server's sphere.ini with `SkillFailGainPercent=50`.
Default -1 preserves legacy behavior. Values 0..100 use the full difficulty
for GAINRADIUS and retain this percentage of normal advancement chance.
0 disables failure gains; 100 gives full normal chance. Input is clamped
to -1..100. `SERV.SKILLFAILGAINPERCENT` exposes the current setting.

Timed and quick failures carry an explicit failure flag. Negative explicit
SKILLGAIN difficulty is also recognized as failure. The percentage applies
once after @SkillGain/@Gain hooks and vetoes. Caps, safe regions, skill locks,
non-advancing failure stages and abort rules remain in effect. GAINRADIUS=0
still disables the radius check. ADV_RATE is unchanged. Positive-radius success weighting is documented in
SKILL_CHALLENGE_GAIN.md.

The updated player event in the scripts repository supplies full recipe
difficulty when enabled. The current event has no legacy half-difficulty
fallback; switching the core to -1 alone does not restore that old script. Deploy that event with the rebuilt core; older binaries do not
recognize the new property. Craft success/exceptional calculations, their UI
percentages, resource use and existing success gain limits are unchanged.

Native regression: 328 checks, including legacy comparisons against this
branch's pre-edit code, plus 3000 exhaustive random-roll evaluations.
Run `python C:/sphere/scripts/.tools/check-skill-fail-gain.py`. These isolated
checks do not start a server. Existing integer rounding, inclusive random
comparison and stat-gain code remain unchanged; existing early returns still
skip stat processing. No live configuration or server was changed.
