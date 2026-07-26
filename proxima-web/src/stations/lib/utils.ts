export const km = (n: number): string => `${(n / 1000).toFixed(1)} km`;
export const pct = (v: number, max: number): number => (max > 0 ? Math.round((v / max) * 100) : 0);
