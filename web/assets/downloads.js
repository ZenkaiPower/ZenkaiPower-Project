(function () {
  'use strict';
  // Verified release links in the README remain usable offline or on API limits.
  var endings = { android: /-android-arm64\.apk$/i, windows: /-windows-x64\.zip$/i, linux: /-linux-x64\.zip$/i };
  fetch('https://api.github.com/repos/ZenkaiPower/ZenkaiPower-Project/releases/latest')
    .then(function (response) { if (!response.ok) throw new Error('Release unavailable'); return response.json(); })
    .then(function (release) {
      var date = document.getElementById('release-date');
      if (date && typeof release.published_at === 'string' && /^\d{4}-\d{2}-\d{2}T/.test(release.published_at)) {
        var year = release.published_at.slice(0, 4);
        date.textContent = '· ' + (year > '2026' ? '2026–' + year : '2026');
        date.hidden = false;
      }
      if (!release.tag_name || !Array.isArray(release.assets)) return;
      Object.keys(endings).forEach(function (platform) {
        var asset = release.assets.find(function (item) { return endings[platform].test(item.name); });
        var target = asset ? asset.browser_download_url : release.html_url;
        if (typeof target !== 'string' || !target.startsWith('https://github.com/ZenkaiPower/ZenkaiPower-Project/releases/')) return;
        document.querySelectorAll('a').forEach(function (link) {
          if (link.dataset.download === platform || endings[platform].test(link.href)) link.href = target;
        });
      });
      var label = document.getElementById('release-version');
      label.textContent = release.tag_name;
      label.hidden = false;
    })
    .catch(function () {});
})();
