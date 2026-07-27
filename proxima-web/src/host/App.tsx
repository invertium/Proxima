import { BootSplash } from './components/BootSplash';
import { Comms } from './components/Comms';
import { Hud } from './components/Hud';
import { Menu } from './components/Menu';
import { useHostRuntime } from './useHostRuntime';

const CONTROLS_HINT =
  'W/S throttle · A/D steer · TAB target · SPACE beam · F torpedo · G dock · J warp · Q/E strafe · ENTER accept · R weld · V alert · ESC menu';

export function App() {
  const { canvasRef, error, evicted, loading, menuProps } = useHostRuntime();

  return (
    <>
      <canvas ref={canvasRef} id="view" className="block h-screen w-screen" />
      <BootSplash loading={loading} error={error} />
      <Hud />
      <Comms />
      <Menu {...menuProps} />
      <div className="pointer-events-none fixed bottom-4 right-4 max-w-[28rem] text-right text-[11px] text-slate-500">
        {CONTROLS_HINT}
      </div>
      {evicted ? (
        <div className="fixed inset-x-0 bottom-0 z-30 border-t border-red-500 bg-red-950 px-4 py-3 text-center text-red-100">
          ANOTHER PILOT WINDOW TOOK OVER THIS SESSION — close this tab, or reload it to take control
          back.
        </div>
      ) : null}
    </>
  );
}
