import Sidebar from "./Sidebar";

const nav = [
  ["overview", "Overview"],
  ["install", "Install"],
  ["build", "Build the native layer"],
  ["run", "Run it"],
  ["examples", "Examples"],
  ["capabilities", "Capabilities"],
  ["elevation", "Elevation"],
  ["safety", "Safety notes"],
  ["verified", "Verified"],
  ["troubleshooting", "Troubleshooting"],
];

export default function Page() {
  return (
    <div className="layout">
      <a href="#main-content" className="skipLink">
        Skip to content
      </a>

      <Sidebar nav={nav} />

      <main className="content" id="main-content">
        <h1>LangBIOS</h1>
        <p className="lede">
          Talk to your computer&apos;s BIOS/UEFI settings in plain English. Talks to real firmware, not a simulation.
        </p>

        <section id="overview">
          <h2>Overview</h2>
          <p>
            <code>native/</code> (C++) is the real engine: a rule-based parser, a dispatcher that routes each
            setting to the right real backend, and platform-specific firmware access. It compiles to
            <code> langbios_native.{"{"}dll,so{"}"}</code> (loaded by Python) and a standalone{" "}
            <code>langbios_cli</code> binary.
          </p>
          <p>
            <code>langbios/</code> (Python) bridges to it via <code>ctypes</code>, adds an optional local-LLM
            fallback (via Ollama) for phrasing the rule parser doesn&apos;t recognize, and ties it together into a
            REPL.
          </p>
          <p>
            There is no mock mode. Reads are safe everywhere; writes change real NVRAM and need elevation.
          </p>
        </section>

        <section id="install">
          <h2>Install</h2>
          <pre>
            <code>{`git clone https://github.com/LAKSHYAJAIN16/LangBIOS.git\ncd LangBIOS`}</code>
          </pre>
          <p>
            The native layer that actually talks to firmware needs a separate build step, below. LangBIOS
            won&apos;t do anything useful until that&apos;s built.
          </p>
        </section>

        <section id="build">
          <h2>Build the native layer</h2>

          <h3>Windows: installer (recommended)</h3>
          <p>
            Builds a real installer wizard (<a href="https://jrsoftware.org/isinfo.php">Inno Setup</a> required)
            that puts <code>langbios.exe</code> on your PATH. No Python needed to use it this way.
          </p>
          <pre>
            <code>{`powershell -ExecutionPolicy Bypass -File native/build.ps1\n& "<Inno Setup install dir>\\ISCC.exe" installer\\LangBIOS.iss\ninstaller\\dist\\LangBIOS-Setup.exe`}</code>
          </pre>
          <p>
            Installs per-user to <code>%LOCALAPPDATA%\Programs\LangBIOS</code> (no admin needed to install) and
            adds it to your PATH. Open a new terminal and run <code>langbios &quot;list settings&quot;</code>{" "}
            directly. Ships a real uninstaller too.
          </p>

          <h3>Windows: build manually</h3>
          <p>Requires a C++20 compiler (clang++/LLVM or MSVC) with the Windows SDK.</p>
          <pre>
            <code>powershell -ExecutionPolicy Bypass -File native/build.ps1</code>
          </pre>
          <p>
            Produces <code>native/build/langbios_native.dll</code> and{" "}
            <code>native/build/langbios_cli.exe</code>, both built for x86_64 by default (works via emulation on
            ARM64 Windows too). Pass <code>-HostArch</code> to build for your machine&apos;s real architecture
            instead.
          </p>

          <h3>Linux</h3>
          <p>Requires g++ or clang++ with C++20 support.</p>
          <pre>
            <code>./native/build.sh</code>
          </pre>
          <p>
            Produces <code>native/build/langbios_native.so</code> and <code>native/build/langbios_cli</code>.
          </p>
          <div className="note">
            Honesty note: the Linux backend was written against the documented kernel ABIs (
            <code>efivarfs</code>, <code>/sys/class/firmware-attributes</code>, <code>/sys/class/tpm</code>) but
            developed on Windows, with no Linux hardware available to verify it against real firmware yet. The
            Windows backend has been verified end-to-end against real hardware. Test the Linux write path
            carefully, starting with read-only commands.
          </div>
        </section>

        <section id="run">
          <h2>Run it</h2>
          <p>If you used the Windows installer, <code>langbios</code> is already on your PATH:</p>
          <pre>
            <code>{`langbios "list settings"\nlangbios                          # interactive REPL`}</code>
          </pre>
          <p>Building from source instead, use the standalone native binary directly:</p>
          <pre>
            <code>{`# Windows\nnative\\build\\langbios_cli.exe "list settings"\n\n# Linux\n./native/build/langbios_cli "list settings"`}</code>
          </pre>
          <p>Or through Python, which adds the local-LLM fallback for phrasing the rule parser misses:</p>
          <pre>
            <code>{`python -m langbios.cli "list settings"\npython -m langbios.cli            # interactive REPL`}</code>
          </pre>
          <p>
            Inside the REPL, type <code>exit</code> or <code>quit</code> to leave. Use <code>--no-llm</code> to
            disable the local-LLM fallback and only use the fast rule-based parser.
          </p>
        </section>

        <section id="examples">
          <h2>Examples</h2>
          <pre>
            <code>{`list settings\nwhat's my secure boot\nenable secure boot\ndisable virtualization\nwhat's my fan profile\nset fan profile to silent\nset power profile to performance\nwhat's my boot order\nset boot order to Windows Boot Manager, USB Storage, Internal Storage\nreset to factory defaults`}</code>
          </pre>
          <p>
            Boot order values must match your machine&apos;s actual boot entry names (run{" "}
            <code>what&apos;s my boot order</code> first to see them). Generic categories like &quot;ssd&quot;/
            &quot;hdd&quot; won&apos;t match anything real.
          </p>
        </section>

        <section id="capabilities">
          <h2>What&apos;s really possible on your hardware</h2>
          <div className="tableWrap">
          <table>
            <thead>
              <tr>
                <th scope="col">Setting</th>
                <th scope="col">Mechanism</th>
                <th scope="col">Writable?</th>
                <th scope="col">Works on</th>
              </tr>
            </thead>
            <tbody>
              <tr>
                <td>
                  <code>boot_order</code>
                </td>
                <td>Standard UEFI variables</td>
                <td>Yes</td>
                <td>Any UEFI machine (elevated)</td>
              </tr>
              <tr>
                <td>
                  <code>secure_boot</code>
                </td>
                <td>Standard UEFI variable</td>
                <td>No, firmware-enforced</td>
                <td>Any UEFI machine (elevated, read-only)</td>
              </tr>
              <tr>
                <td>
                  <code>tpm</code>
                </td>
                <td>WMI / sysfs</td>
                <td>No, not OS-writable anywhere</td>
                <td>Any machine with a TPM (elevated)</td>
              </tr>
              <tr>
                <td>
                  <code>virtualization</code>, <code>fan_profile</code>, <code>power_profile</code>,{" "}
                  <code>cpu_turbo</code>, <code>fast_boot</code>, <code>xmp</code>
                </td>
                <td>Vendor WMI (Windows) / firmware-attributes (Linux)</td>
                <td>Yes, if the vendor stack is present</td>
                <td>Dell / HP / Lenovo only</td>
              </tr>
            </tbody>
          </table>
          </div>
          <p>
            On anything else, LangBIOS reports &quot;no vendor BIOS management interface available&quot; rather
            than pretending a setting changed.
          </p>
        </section>

        <section id="elevation">
          <h2>Elevation / permissions</h2>
          <ul>
            <li>
              <strong>Windows:</strong> needs <code>SeSystemEnvironmentPrivilege</code>, only ever granted to
              Administrators. Run from an elevated terminal, or enable <code>sudo</code> under Settings &rarr;
              Privacy &amp; Security &rarr; For developers.
            </li>
            <li>
              <strong>Linux:</strong> needs root / <code>CAP_SYS_ADMIN</code>. Run with <code>sudo</code>.
            </li>
          </ul>
          <p>
            Non-elevated runs still work and explain exactly why an operation needs elevation, instead of
            failing silently.
          </p>

          <h3>Local LLM fallback (optional)</h3>
          <p>For phrasing the rule parser doesn&apos;t recognize:</p>
          <pre>
            <code>{`ollama pull llama3.2\nollama serve`}</code>
          </pre>
          <pre>
            <code>{`LANGBIOS_OLLAMA_URL=http://localhost:11434\nLANGBIOS_OLLAMA_MODEL=llama3.2\nLANGBIOS_OLLAMA_TIMEOUT=10`}</code>
          </pre>
        </section>

        <section id="safety">
          <h2>Safety notes</h2>
          <ul>
            <li>
              <strong>Boot order writes refuse partial reorders.</strong> Every current entry must be listed, in
              order, so a typo can&apos;t silently drop a boot device.
            </li>
            <li>
              <strong>Secure Boot and TPM are always read-only</strong> from every code path, by firmware design.
            </li>
            <li>
              <strong>Vendor attribute writes</strong> may only take effect after a reboot / pending-changes
              commit, and can fail if a BIOS admin password is set; both are reported explicitly.
            </li>
          </ul>
        </section>

        <section id="verified">
          <h2>Verified against real hardware</h2>
          <p>
            On this project&apos;s own dev machine (Microsoft Surface, Snapdragon/ARM64, no vendor BIOS
            management stack):
          </p>
          <ul>
            <li>
              Real elevated read + write round-trip of the actual <code>BootOrder</code> UEFI variable (read
              current order, write the same order back, read again to confirm), genuinely persisted to
              firmware.
            </li>
            <li>Real elevated read of Secure Boot state and WMI-based TPM query.</li>
            <li>Correct, honest non-elevated error messages when not run as Administrator.</li>
            <li>Correct &quot;no vendor BIOS interface available&quot; report, since this hardware has none.</li>
          </ul>
        </section>

        <section id="troubleshooting">
          <h2>Troubleshooting</h2>
          <p>
            <strong>&quot;Native library not found&quot;:</strong> you haven&apos;t run the build script yet,
            or built it for the wrong architecture.
          </p>
          <p>
            <strong>&quot;requires SeSystemEnvironmentPrivilege&quot; / &quot;requires running as root&quot;:</strong>{" "}
            expected outside an elevated/root shell. Re-run elevated.
          </p>
          <p>
            <strong>&quot;No vendor BIOS management interface available&quot;:</strong> your machine
            isn&apos;t Dell/HP/Lenovo (or the Linux driver isn&apos;t loaded). Real hardware limitation, not a
            bug.
          </p>
          <p>
            Still stuck?{" "}
            <a href="https://github.com/LAKSHYAJAIN16/LangBIOS/issues/new">Open an issue.</a>
          </p>
        </section>
      </main>
    </div>
  );
}
