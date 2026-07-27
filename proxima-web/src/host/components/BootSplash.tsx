interface Props {
  loading: boolean;
  error: Error | null;
}

export function BootSplash({ loading, error }: Props) {
  if (!loading && !error) return null;

  return (
    <div
      id="boot"
      className="fixed inset-0 z-10 grid place-content-center gap-2 bg-[#05070f] text-center"
    >
      <h1 className="text-3xl font-medium tracking-[0.3em] text-[#7dd3fc]">PROXIMA</h1>
      {error ? (
        <p className="text-sm text-[#f87171]">
          asset load failed
          <br />
          <code className="text-xs">{String(error)}</code>
        </p>
      ) : null}
    </div>
  );
}
