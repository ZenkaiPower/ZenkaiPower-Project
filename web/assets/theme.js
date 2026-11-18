(function () {
  'use strict';
  var preference = 'system';
  var media = typeof window.matchMedia === 'function' ? window.matchMedia('(prefers-color-scheme: light)') : null;
  try { preference = localStorage.getItem('zenkai-theme') || 'system'; } catch (_) {}
  if (!['system', 'light', 'dark'].includes(preference)) preference = 'system';
  function apply() {
    document.documentElement.dataset.theme = preference === 'system' ? (media && media.matches ? 'light' : 'dark') : preference;
  }
  apply(); // Run before CSS to avoid flashing the opposite theme.
  if (media && media.addEventListener) media.addEventListener('change', apply);
  document.addEventListener('DOMContentLoaded', function () {
    var buttons = document.querySelectorAll('[data-theme-choice]');
    function updateButtons() {
      buttons.forEach(function (button) {
        button.setAttribute('aria-pressed', String(button.dataset.themeChoice === preference));
      });
    }
    updateButtons();
    buttons.forEach(function (button) {
      button.addEventListener('click', function () {
        preference = button.dataset.themeChoice;
        try { localStorage.setItem('zenkai-theme', preference); } catch (_) {}
        apply();
        updateButtons();
      });
    });
  });
})();
