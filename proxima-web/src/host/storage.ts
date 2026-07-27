// Browser persistence for the campaign save.
//
// IndexedDB rather than localStorage: localStorage is synchronous and blocks the main
// thread, and a bridge sim saving mid-fight should never cost a frame. The API is
// promise-based so the caller can fire-and-forget.

import { migrate, type SaveGame } from '../sim/save';

const DB_NAME = 'proxima';
const STORE = 'campaign';
const KEY = 'save';

const open = (): Promise<IDBDatabase> =>
  new Promise((resolve, reject) => {
    const req = indexedDB.open(DB_NAME, 1);
    req.onupgradeneeded = () => req.result.createObjectStore(STORE);
    req.onsuccess = () => resolve(req.result);
    req.onerror = () => reject(req.error);
  });

const tx = async <T>(
  mode: IDBTransactionMode,
  run: (store: IDBObjectStore) => IDBRequest<T>,
): Promise<T> => {
  const db = await open();
  return new Promise<T>((resolve, reject) => {
    const request = run(db.transaction(STORE, mode).objectStore(STORE));
    request.onsuccess = () => resolve(request.result);
    request.onerror = () => reject(request.error);
  }).finally(() => db.close());
};

export const saveCampaign = async (save: SaveGame): Promise<void> => {
  try {
    await tx('readwrite', (store) => store.put(save, KEY));
  } catch {
    // A failed save must never take the game down with it — private-mode browsers
    // and full quotas both land here. The crew keeps playing; progress just isn't kept.
  }
};

export const loadCampaign = async (): Promise<SaveGame | null> => {
  try {
    return migrate(await tx('readonly', (store) => store.get(KEY)));
  } catch {
    return null;
  }
};

export const clearCampaign = async (): Promise<void> => {
  try {
    await tx('readwrite', (store) => store.delete(KEY));
  } catch {
    // Same reasoning as saveCampaign.
  }
};
