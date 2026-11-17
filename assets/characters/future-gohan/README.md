# Future Gohan: one-armed outfits

The manifest adds outfits 3 and 4 for Future Gohan (159) and Super Saiyan (160):

- Outfit 3: orange gi and blue undershirt.
- Outfit 4: yellow gi and blue undershirt, matching outfit 2's colors.

Both outfits have a missing left arm and retain the blue T-shirt sleeve.
Each form includes separate clean starting and torn battle-damage models.
The alternate colors share the same geometry, UVs and damage states.

The `.bin` files are the model assets used by compilation. They contain visual
pack entries 3 through 17. The manifest registers each asset with its resource
slot. The content loader preserves portraits, animations, combat parameters,
voices and skill text from the installed game's base resources.

These additional outfits require `PORT_ZENKAI_EXTRA_OUTFITS=1` and an active
offline Zenkai pack. Classic Mode and online play use only the original two
outfits. Disabling the build flag excludes the embedded outfit assets.
