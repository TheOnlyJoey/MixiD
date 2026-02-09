{
  description = "Unofficial Linux control panel for the Audient iD series interfaces";
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = {flake-parts, ...} @ inputs:
    flake-parts.lib.mkFlake {inherit inputs;} {
      systems = ["x86_64-linux"];
      imports = [flake-parts.flakeModules.easyOverlay];

      perSystem = {
        config,
        lib,
        pkgs,
        ...
      }: let
        inherit (lib) getExe;

        imgui =
          pkgs.runCommandLocal "imgui-git" {
            src = pkgs.fetchzip {
              url = "https://github.com/ocornut/imgui/archive/refs/tags/v1.92.4.tar.gz";
              hash = "sha256-DyQ2fh749S41UFdLto7TtxsnBsd7CBzAUFq36LeZZ5Y=";
            };
          } ''
            mkdir -p $out
            cp -rt $out/ $src/backends $src/misc $src/*.h $src/*.cpp
          '';

        mixid = pkgs.stdenv.mkDerivation {
          pname = "mixid";
          version = "0.1.6";

          src = ../.;

          cmakeFlags = [
            "-DCMAKE_BUILD_TYPE=Release"
            "-DIMGUI_RELEASE_DIR=${imgui}"
          ];

          nativeBuildInputs = with pkgs; [
            copyDesktopItems
            cmake
            pkg-config
            udevCheckHook
          ];

          buildInputs = with pkgs; [
            glew
            glfw
            libGL
            libusb1
            libusb-compat-0_1
            libx11
            libxrandr
            wayland
          ];

          desktopItems = [
            (pkgs.makeDesktopItem {
              name = "mixid";
              desktopName = "MixiD";
              genericName = "Audio Mixer";
              comment = "Unofficial Linux control panel for the Audient iD series interfaces";
              exec = "MixiD";
              icon = "pulseeffects";
              categories = ["Audio" "Utility"];
              startupWMClass = "MixiD";
            })
          ];

          installPhase = let
            audientRules = pkgs.writeText "84-audient.rules" ''
              SUBSYSTEM=="usb", ATTR{idVendor}=="2708", MODE="0666", GROUP="audio"
              SUBSYSTEM=="usb", ATTR{idVendor}=="2708", MODE="0666", GROUP="plugdev"
            '';

            unwrapped = ".mixid-unwrapped";
            wrapper = pkgs.writeScript "mixid-wrapper" ''
              #!${getExe pkgs.dash}

              if [ -n ''${XDG_CONFIG_HOME+x} ]; then
                cd "$XDG_CONFIG_HOME"
              else
                cd "$HOME/.config"
              fi

              ''${0%/*}/${unwrapped} "$@"
            '';
          in ''
            runHook preInstall

            install -Dm644 ${audientRules} $out/lib/udev/rules.d/84-audient.rules
            install -Dm644 -t $out/lib/ libimgui.a
            install -Dm755 MixiD $out/bin/${unwrapped}
            install -Dm755 ${wrapper} $out/bin/MixiD

            runHook postInstall
          '';

          meta = {
            homepage = "https://github.com/TheOnlyJoey/MixiD";
            description = "Unofficial Linux control panel for the Audient iD series interfaces";
            license = lib.licenses.gpl3;
            platforms = lib.platforms.linux;
            mainProgram = "MixiD";
          };
        };
      in {
        overlayAttrs = {inherit (config.packages) mixid;};

        packages = {
          inherit mixid;
          default = mixid;
        };

        devShells.default = pkgs.mkShell {
          packages = with mixid; nativeBuildInputs ++ buildInputs;
        };
      };
    };
}
