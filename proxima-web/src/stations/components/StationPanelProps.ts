import type { Command } from '@/sim/types';

export interface StationPanelProps {
  readonly send: (cmd: Command, id?: number) => void;
}
