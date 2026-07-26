// Player settings, persisted locally.
//
// Small, but two of these are not cosmetic: BridgeAudio.setVolume has existed since
// the audio bus was written and nothing ever called it, and a phone that sleeps
// mid-fight takes a crew station down with it.

export interface Settings {
  /** Master volume, 0..1. */
  volume: number;
  /** Renderer pixel-ratio cap. Lower is faster on a weak GPU. */
  quality: 'low' | 'medium' | 'high';
  /** Keep the screen awake on crew consoles. */
  wakeLock: boolean;
}

const KEY = 'proxima.settings';

export const DEFAULTS: Settings = { volume: 0.5, quality: 'high', wakeLock: true };

export const PIXEL_RATIO_CAP: Record<Settings['quality'], number> = {
  low: 1,
  medium: 1.5,
  high: 2,
};

export const loadSettings = (): Settings => {
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return { ...DEFAULTS };
    // Merge over defaults so a settings file from an older build still loads.
    return { ...DEFAULTS, ...(JSON.parse(raw) as Partial<Settings>) };
  } catch {
    return { ...DEFAULTS };
  }
};

export const saveSettings = (settings: Settings): void => {
  try {
    localStorage.setItem(KEY, JSON.stringify(settings));
  } catch {
    // Private mode or a full quota. Losing a preference must not break the game.
  }
};

/**
 * Holds a screen wake lock while the page is visible, re-acquiring after the browser
 * drops it (which it does on every tab switch). Unsupported browsers no-op.
 */
export const keepAwake = (enabled: () => boolean): void => {
  if (!('wakeLock' in navigator)) return;

  let lock: WakeLockSentinel | null = null;

  const acquire = async (): Promise<void> => {
    if (document.hidden || !enabled() || lock) return;
    try {
      lock = await navigator.wakeLock.request('screen');
      lock.addEventListener('release', () => {
        lock = null;
      });
    } catch {
      // Denied, or the document isn't allowed one. Not worth surfacing.
    }
  };

  document.addEventListener('visibilitychange', () => {
    if (document.hidden) {
      void lock?.release();
      lock = null;
    } else {
      void acquire();
    }
  });

  void acquire();
};
