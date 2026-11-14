# Project website

The site is published at <https://zenkaipower.github.io/ZenkaiPower-Project/>.
Its content comes from the repository's root `README.md`; edit that file to
update the page. The layout and styling live in `web/`.

In the GitHub repository, select **Settings → Pages → Build and deployment →
Source → GitHub Actions**. The Website workflow builds pull requests and deploys
changes from `main`. It can also be run manually from the Actions tab.

`prepare.py` stages the root README, `figs/`, website templates and application
icon into a new directory. It leaves local `assets/setup/` artwork out of the
website. Jekyll builds this staged directory, and `check.py` checks the generated
homepage's links, images and navigation anchors before deployment.

With Python and Jekyll installed, a local build can be checked with:

```sh
python web/prepare.py port/build/website-source
jekyll build --source port/build/website-source --destination port/build/website
python web/check.py port/build/website
```

Use a new staging directory for each build. To preview, serve `port/build/`
with a local HTTP server after building to `port/build/ZenkaiPower-Project`,
so the configured project URL prefix is preserved.
