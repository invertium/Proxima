import { useState } from 'react';

import { Slider } from '@/components/ui/slider';

/**
 * A slider whose authoritative value comes from the 60 Hz snapshot, but which the hand
 * owns while a finger is on it.
 *
 * A fully controlled slider fed straight from the snapshot fights the drag: every frame
 * the sim echoes back a value one relay round-trip behind where the thumb now is, so the
 * thumb snaps backwards under the finger. That is the judder fixed in 5f1becb, and
 * binding `value` directly to the snapshot re-introduces it — over LAN latency on a
 * phone, which is the case that matters and the one a desktop dev never sees.
 *
 * So: while dragging, the local value wins and every change is still sent; on release
 * the latch clears and the snapshot takes the lever back. `onValueCommitted` fires on
 * pointerup and keyup, so keyboard operation behaves the same way.
 */
export function LiveSlider({
  value,
  onChange,
  min,
  max,
  step,
  label,
}: {
  /** Authoritative position, from the snapshot. Ignored while a drag is in progress. */
  readonly value: number;
  readonly onChange: (v: number) => void;
  readonly min: number;
  readonly max: number;
  readonly step: number;
  readonly label: string;
}) {
  const [drag, setDrag] = useState<number | null>(null);

  return (
    <Slider
      aria-label={label}
      value={[drag ?? Math.max(min, Math.min(max, value))]}
      onValueChange={(next) => {
        const v = Array.isArray(next) ? next[0] : next;
        if (v === undefined) return;
        setDrag(v);
        onChange(v);
      }}
      onValueCommitted={() => setDrag(null)}
      min={min}
      max={max}
      step={step}
    />
  );
}
