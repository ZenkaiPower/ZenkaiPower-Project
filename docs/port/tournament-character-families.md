# Tournament character identities and transformation families

Status: implemented mapping for the Zenkai Dragon World Tour rules in [port_tour_rules.h](../../include/menu/port_tour_rules.h). This covers all **161 roster IDs (0-160)**. Names were checked against the local Character Reference text; identity and family assignments below are tournament selection policy, not a claim about the original game's duplicate checks or transformation links.

## Selection priority

For each generated CPU entrant, rank eligible entries in this order:

1. **New identity:** neither this character nor any incarnation/form of it is represented.
2. **New family of an existing identity:** another incarnation or standalone variant of an already represented character.
3. **New form of an existing family:** another transformation from an already represented incarnation.
4. **Additional Saibamen, Cell Juniors, and Frieza Soldiers:** fill remaining slots after exhausting the other eligible choices.

Use a lower tier only when no candidate remains in a higher tier. Track player-selected entrants and reserve the selected boss's identity/family when applying these preferences, so generated entrants prefer other identities. These are preferences: fallback tiers permit a full bracket when the eligible pool contains fewer identities than slots. Exact roster IDs remain unique except for the individual participant slots defined below.

Apply these tiers inside the eligible pool from [Dragon World Tour changes](dragon-world-tour-changes.md). Neither a fallback tier nor a boss reservation grants access to an ineligible entry. Tournament cost preferences must not defeat the identity-first ordering; relax cost preferences within the better diversity tier before moving to a worse tier. Within the same tier, prefer identities/families with fewer representatives before random selection.

Example: after selecting Goku (Mid), prefer Krillin over Goku (End); if no new identity remains, prefer Goku (End) over Goku (Mid) Super Saiyan.

The CPU considers a giant form only after its eligible non-giant transformation forms are already represented or unavailable. Standalone giants such as Dr. Wheelo and Hirudegarn follow the normal selection priorities.

## Mapping conventions

- **Identity** groups incarnations that should compete for the same first-priority slot. Goku (Early/Mid/End/GT/Kid) shares Goku. Gohan and Future Gohan are separate identities; adult Trunks (Sword/Fighting) and Kid Trunks are separate identities.
- **Family** groups the forms of one incarnation. Standalone variants such as Majin Vegeta, Ultimate Gohan, Great Saiyaman, Mecha Frieza, Metal Cooler, and Cyborg Tao are separate families under their character's identity.
- Gotenks, Vegito, and Gogeta are distinct fusion identities, separate from their components and from one another. Both Gogetas share the Gogeta identity. Kibito Kai is a separate fusion identity from Supreme Kai.
- Videl and Great Saiyaman 2 share an identity. Kid/Teen/Adult/Ultimate Gohan and Great Saiyaman share Gohan. Future Gohan and his Super Saiyan form share their own identity. Grandpa Gohan is a separate person and identity.
- Majin Buu (Fat) is a separate identity from Buu (Evil/Super/Kid). Evil, Super, and Kid Buu remain separate families within that shared Buu identity. Uub/Majuub share Uub, separate from both Buu identities.
- Baby's Vegeta-host forms share Baby, separate from Vegeta. Super 17 is its own fusion identity, separate from Android 17. Demon King Piccolo is separate from Piccolo. King Vegeta is separate from Vegeta; King Cold is separate from Frieza.
- Cell Jr. is separate from Cell. Nuova Shenron is separate from Syn/Omega Shenron. Saibamen, Cell Jr., Frieza Soldier, and Appule have the individual-participant exceptions below.
- A singleton has one identity, one family, and one form in the base mapping. Family labels describe selection grouping; costumes are not separate roster entries, but Frieza Soldier and Appule costumes create distinct tournament participant slots.

## Individual participant exceptions

These four roster entries represent multiple individuals for tournament selection. Expand an eligible entry into the following participant slots before applying diversity priorities:

| Roster ID | Character | Individual slots | Maximum participants per tournament |
|---:|---|---|---|
| 79 | Saibamen | Saibaman 1 through Saibaman 5 | 5 |
| 109 | Cell Jr. | Cell Junior 1 through Cell Junior 5 | 5 |
| 99 | Frieza Soldier | One individual for each valid outfit/color | Number of valid outfits/colors |
| 98 | Appule | One individual for each valid outfit/color | Number of valid outfits/colors |

Saibamen, Cell Juniors, and Frieza Soldiers each have one chance at **new-identity priority**. Additional entrants of these types fill gaps once the bracket runs out of other eligible choices. Each eligible Appule outfit keeps its own chance at new-identity priority.

Saibamen and Cell Juniors use an occurrence index to distinguish participants; they may repeat the same exact character/form ID and outfit. Frieza Soldier and Appule use the pair **roster ID + outfit/color ID**; each pair may appear only once. Obtain valid outfit counts from the game's character costume data (`ChrTbl_WrapCostume`), rather than assuming a fixed number or inventing additional colors.

Limits apply across the whole tournament, including player-selected and generated entrants and any reserved boss. Player selections consume the corresponding participant slots. Each slot can be used once, and selection must retain the chosen outfit through the bracket and battle setup. Other roster entries keep their exact-ID duplicate restriction.

The base identity mapping below still contains 83 identities and 104 families. The exception slots are a runtime expansion of four mapped entries, not extra roster IDs. Only expand characters already in the eligible CPU pool; this does not unlock them or bypass saga eligibility or the original-132 exception.

## Complete roster mapping

Each roster ID occurs exactly once. The pair **Identity + Family** identifies a transformation family. Different rows with the same identity are alternate incarnations/variants and receive second priority after that identity is represented.

| Identity | Family / incarnation | Roster entries (ID: name) |
|---|---|---|
| Goku | Early | `0`: Goku (Early) |
| Goku | Mid | `1`: Goku (Mid); `2`: Goku (Mid) Super Saiyan |
| Goku | End | `3`: Goku (End); `4`: Goku (End) Super Saiyan; `5`: Goku (End) Super Saiyan 2; `6`: Goku (End) Super Saiyan 3 |
| Goku | GT | `7`: Goku (GT); `8`: Goku (GT) Super Saiyan; `9`: Goku (GT) Super Saiyan 3; `10`: Goku (GT) Super Saiyan 4 |
| Goku | Kid | `11`: Kid Goku; `12`: Kid Goku (Great Ape) |
| Gohan | Kid | `13`: Kid Gohan |
| Gohan | Teen | `14`: Teen Gohan; `15`: Teen Gohan Super Saiyan; `16`: Teen Gohan Super Saiyan 2 |
| Gohan | Adult | `17`: Gohan (Adult); `18`: Gohan (Adult) Super Saiyan; `19`: Gohan (Adult) Super Saiyan 2 |
| Gohan | Great Saiyaman | `20`: Great Saiyaman |
| Gohan | Ultimate | `21`: Ultimate Gohan |
| Future Gohan | Standard | `159`: Future Gohan; `160`: Future Gohan Super Saiyan |
| Piccolo | Early | `22`: Piccolo (Early) |
| Piccolo | End | `23`: Piccolo (End) |
| Nail | Standard | `24`: Nail |
| Krillin | Standard | `25`: Krillin |
| Yamcha | Standard | `26`: Yamcha |
| Tien | Standard | `27`: Tien |
| Chiaotzu | Standard | `28`: Chiaotzu |
| Vegeta | Scouter | `29`: Vegeta (Scouter); `30`: Great Ape Vegeta |
| Vegeta | Mid | `31`: Vegeta; `32`: Vegeta Super Saiyan; `33`: Super Vegeta |
| Vegeta | End | `34`: Vegeta (End); `35`: Vegeta (End) Super Saiyan; `36`: Vegeta (End) Super Saiyan 2 |
| Vegeta | Majin | `37`: Majin Vegeta |
| Vegeta | Super Saiyan 4 | `38`: Vegeta Super Saiyan 4 |
| Trunks | Sword | `39`: Trunks (Sword); `40`: Trunks (Sword) Super Saiyan |
| Trunks | Fighting | `41`: Trunks; `42`: Trunks Super Saiyan; `43`: Super Trunks |
| Kid Trunks | Standard | `44`: Kid Trunks; `45`: Kid Trunks Super Saiyan |
| Goten | Standard | `46`: Goten; `47`: Goten Super Saiyan |
| Gotenks | Standard | `48`: Gotenks; `49`: Gotenks Super Saiyan; `50`: Gotenks Super Saiyan 3 |
| Vegito | Standard | `51`: Vegito; `52`: Super Vegito |
| Gogeta | Super Gogeta | `53`: Super Gogeta |
| Gogeta | Super Saiyan 4 | `54`: Gogeta Super Saiyan 4 |
| Mr. Satan / Hercule | Standard | `55`: Mr. Satan / Hercule |
| Videl | Standard | `56`: Videl |
| Videl | Great Saiyaman 2 | `57`: Great Saiyaman 2 |
| Supreme Kai | Standard | `58`: Supreme Kai |
| Kibito Kai | Standard | `59`: Kibito Kai |
| Yajirobe | Standard | `60`: Yajirobe |
| Pikkon | Standard | `61`: Pikkon |
| Tapion | Standard | `62`: Tapion |
| Pan | Standard | `63`: Pan |
| Uub | Standard | `64`: Uub; `65`: Majuub |
| Master Roshi | Standard | `66`: Master Roshi; `67`: Master Roshi Full Power |
| Grandpa Gohan | Standard | `68`: Grandpa Gohan |
| Nam | Standard | `69`: Nam |
| Android #8 | Standard | `70`: Android #8 |
| King Vegeta | Standard | `71`: King Vegeta; `72`: Great Ape King Vegeta |
| Bardock | Standard | `73`: Bardock; `74`: Great Ape Bardock |
| Fasha | Standard | `75`: Fasha; `76`: Great Ape Fasha |
| Raditz | Standard | `77`: Raditz; `78`: Great Ape Raditz |
| Saibamen | Standard | `79`: Saibamen |
| Nappa | Standard | `80`: Nappa; `81`: Great Ape Nappa |
| Zarbon | Standard | `82`: Zarbon; `83`: Zarbon Post-Transformation |
| Dodoria | Standard | `84`: Dodoria |
| Cui | Standard | `85`: Cui |
| Captain Ginyu | Standard | `86`: Captain Ginyu |
| Recoome | Standard | `87`: Recoome |
| Burter | Standard | `88`: Burter |
| Jeice | Standard | `89`: Jeice |
| Guldo | Standard | `90`: Guldo |
| Frieza | Organic | `91`: Frieza 1st Form; `92`: Frieza 2nd Form; `93`: Frieza 3rd Form; `94`: Frieza Final Form; `95`: Frieza (100% Power) |
| Frieza | Mecha | `96`: Mecha Frieza |
| King Cold | Standard | `97`: King Cold |
| Appule | Standard | `98`: Appule |
| Frieza Soldier | Standard | `99`: Frieza Soldier |
| Android #16 | Standard | `100`: Android #16 |
| Android #17 | Standard | `101`: Android #17 |
| Android #18 | Standard | `102`: Android #18 |
| Android #19 | Standard | `103`: Android #19 |
| Dr. Gero / Android 20 | Standard | `104`: Dr. Gero / Android 20 |
| Cell | Standard | `105`: Cell 1st Form; `106`: Cell 2nd Form; `107`: Perfect Cell; `108`: Super Perfect Cell |
| Cell Jr. | Standard | `109`: Cell Jr. |
| Babidi | Standard | `110`: Babidi |
| Demon King Dabura | Standard | `111`: Demon King Dabura |
| Majin Buu (Fat) | Standard | `112`: Majin Buu |
| Buu | Evil Buu | `113`: Evil Buu |
| Buu | Super Buu | `114`: Super Buu; `115`: Super Buu Gotenks Absorbed; `116`: Super Buu Gohan Absorbed |
| Buu | Kid Buu | `117`: Kid Buu |
| Garlic Jr. | Standard | `118`: Garlic Jr.; `119`: Super Garlic Jr. |
| Dr. Wheelo | Standard | `120`: Dr. Wheelo |
| Turles | Standard | `121`: Turles; `122`: Great Ape Turles |
| Slug | Standard | `123`: Slug; `124`: Giant Slug |
| Salza | Standard | `125`: Salza |
| Cooler | Organic | `126`: Cooler; `127`: Cooler Final Form |
| Cooler | Metal | `128`: Metal Cooler |
| Android 13 | Standard | `129`: Android 13; `130`: Fusion Android 13 |
| Broly | Standard | `131`: Broly; `132`: Broly Super Saiyan; `133`: Broly Legendary Super Saiyan |
| Zangya | Standard | `134`: Zangya |
| Bojack | Standard | `135`: Bojack; `136`: Full Power Bojack |
| Janemba | Standard | `137`: Janemba; `138`: Super Janemba |
| Hirudegarn | Standard | `139`: Hirudegarn |
| Baby | Vegeta host | `140`: Baby Vegeta; `141`: Super Baby 1; `142`: Super Baby 2; `143`: Great Ape Baby |
| Super 17 | Standard | `144`: Super 17 |
| Nuova Shenron | Standard | `145`: Nuova Shenron |
| Syn Shenron | Standard | `146`: Syn Shenron; `147`: Omega Shenron |
| Tao Pai Pai | Human | `148`: Tao Pai Pai |
| Tao Pai Pai | Cyborg | `149`: Cyborg Tao |
| General Blue | Standard | `150`: General Blue |
| Devilman | Standard | `151`: Devilman |
| Pilaf Machine | Standard | `152`: Pilaf Machine; `153`: Pilaf Machine Fusion |
| Tambourine | Standard | `154`: Tambourine |
| Demon King Piccolo | Standard | `155`: Demon King Piccolo |
| Arale | Standard | `156`: Arale |
| Chi-Chi | Standard | `157`: Chi-Chi |
| Spopovich | Standard | `158`: Spopovich |

## Implementation checks

- The mapping contains 83 identities and 104 families across 161 roster entries.
- Five Saibamen and five Cell Juniors can participate, including repeated outfits; a sixth of either cannot.
- Frieza Soldier and Appule each permit one participant per valid outfit/color; a repeated outfit for the same roster ID cannot participate twice.
- Extra Saibamen, Cell Juniors, and Frieza Soldiers appear only when other eligible choices run out. Appule outfits receive independent new-identity chances. Player selections consume slots and count toward the limits.
- Gohan/Future Gohan, adult Trunks/Kid Trunks, and Fat Buu/Buu (Evil/Super/Kid) are independent identities: selecting one does not lower the other's new-identity priority.
- Cover IDs 0-160 exactly once; reject list markers 161-164 as character entries.
- Different Goku incarnations share one identity, while their individual transformation families remain distinct.
- With Goku (Mid) already present, an eligible unused identity wins over both Goku (End) and Goku (Mid) Super Saiyan, even if the latter better matches the cost preference.
- If all eligible identities are represented, an unused Goku family wins over another form of an already represented Goku family.
- If every eligible family is represented, unused exact forms can fill remaining slots without leaving the eligible pool. Exception characters obey their slot limits instead of exact-ID uniqueness.
- Reserving a boss identity influences preference, not eligibility; a different incarnation/form of that boss is still a fallback candidate if needed.
- The original 132-entry exception changes the eligible pool only; it does not disable these diversity preferences.
- Original mode retains its current exact-ID selection behavior.

## Sources

- [Dragon World Tour changes](dragon-world-tour-changes.md): eligibility, boss candidates, tournament gates, and the original-roster exception.
- [Dragon History progression](dragon-history-progression.md): saga reward groups.
- [Bracket_FillEntrants](../../src/menu/menu_k_f.c): current exact-ID duplicate comparison.
- Character names/forms: local US archive file `0x1D0`, section 1, unpacked sections 22 and 23; spelling normalized for readable labels.
