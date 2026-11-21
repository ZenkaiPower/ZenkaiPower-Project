# Project website

The site is published at <https://zenkaipower.github.io/ZenkaiPower-Project/>.
Its content comes from the repository's root `README.md`; edit that file to
update the page. The layout and styling live in `web/`. Four feature columns
show Tenkaichi3Decomp, Added Features, Dragon Net Battle, and Mobile Version.
Sparking! Changes appears below them as a full-width section. Desktop windows
distribute its `###` subsections across automatically balanced columns, keeping
each feature together. Desktop windows
wider than 1100px display all four columns. The project header stays at the top while scrolling. Contents stays fixed on
the left on desktop, with its own scrolling when needed; on mobile it appears
above the content. Smaller screens use three, two, or one column.

The carousel is generated from README GIFs with `data-carousel-caption`:

```html
<img src="./figs/game-mode.gif" width="320"
     data-carousel-caption="Switch between game editions."/>
```

The caption becomes the carousel subtitle. Only tagged GIFs are included.
Slide duration is read automatically from the GIF's frame delays for one full
animation cycle, with a minimum of 5000 milliseconds for short loops.
`data-carousel-duration` is an optional override in milliseconds.
Edit, add, or remove these attributes in the root README; the preview rebuilds
automatically. Captions are plain text. GIFs marked as 32px or 64px wide retain
their small-icon presentation inside the 16:9 frame.

In the GitHub repository, select **Settings → Pages → Build and deployment →
Source → GitHub Actions**. The Website workflow builds pull requests and deploys
changes from `main`. It can also be run manually from the Actions tab.

`prepare.py` stages the root README, `figs/`, website templates and application
icon into a new directory. It leaves local `assets/setup/` artwork out of the
website. Jekyll builds this staged directory, and `check.py` checks the generated
homepage's links, images and navigation anchors before deployment.

The website layout includes download buttons with monochrome SVG icons for
Android ARM64, Windows x64, and Linux x64. They appear only on Pages, not in
the repository README. The links work without JavaScript. On the website,
`assets/downloads.js` refreshes them from GitHub's latest published release;
the layout links remain the fallback if the API is unavailable.

The site currently always uses dark mode and hides the theme buttons. Set
`theme_switcher_enabled: true` in `_config.yml` and rebuild to re-enable them.
When enabled, the System, Dark and Light theme buttons follow the system/browser preference by default and use
dark when no preference is identified. A manual Light or Dark choice is saved
in the browser. `assets/theme.js` applies it before the stylesheet loads.

With Python, Pillow, and Jekyll installed, a local build can be checked with:

```sh
python -m pip install Pillow
python web/prepare.py port/build/website-source
jekyll build --source port/build/website-source --destination port/build/website
python web/check.py port/build/website
```

Use a new staging directory for each build. To preview, serve `port/build/`
with a local HTTP server after building to `port/build/ZenkaiPower-Project`,
so the configured project URL prefix is preserved.

For a Docker preview, first rebuild the project image after changes to the
Dockerfile. From the repository root in PowerShell, run:

```powershell
docker build --platform linux/amd64 -t zenkaipower .
docker ps -q --filter "ancestor=zenkaipower" --filter "publish=4000" | ForEach-Object { docker stop $_ }
docker ps -aq --filter "name=^/zenkaipower-pages-preview$" | ForEach-Object { docker rm -f $_ }
docker run --rm --name zenkaipower-pages-preview --platform linux/amd64 -p 4000:4000 -v "${PWD}:/work" --entrypoint python3 zenkaipower -u web/preview.py
```

Open <http://localhost:4000/ZenkaiPower-Project/>. Press Ctrl+C to stop the
server. Changes to the README, website files, figures, or icon trigger a rebuild;
refresh the browser to see them. If another application uses port 4000, stop it
or change the host port in the `-p` option and the printed URL.
