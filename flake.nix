{
  description = "Build tools for the montezuma checkout";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";
  inputs.nixpkgs-emscripten.url = "github:NixOS/nixpkgs/5f9599e05cb87b4f85b9d06d6f2a76e1aef9c183";

  outputs =
    { nixpkgs, nixpkgs-emscripten, ... }:
    let
      forAll = nixpkgs.lib.genAttrs [
        "x86_64-linux"
        "aarch64-linux"
      ];
    in
    {
      packages = forAll (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          # Android SDK for the Android export (WP-36): versions from the
          # pinned Godot's android_source.zip config.gradle.
          androidPkgs = import nixpkgs {
            inherit system;
            config = {
              allowUnfree = true;
              android_sdk.accept_license = true;
            };
          };
          androidSdk = (androidPkgs.androidenv.composeAndroidPackages {
            platformVersions = [ "36" ];
            buildToolsVersions = [ "36.1.0" ];
            includeNDK = true;
            ndkVersions = [ "29.0.14206865" ];
            includeEmulator = true;
            includeSystemImages = true;
            systemImageTypes = [ "google_apis" ];
            abiVersions = [ "x86_64" ];
          }).androidsdk;
        in
        {
          default = pkgs.stdenvNoCC.mkDerivation {
            name = "montezuma-tools";
            dontUnpack = true;
            installPhase = "mkdir -p $out";
            ANDROID_HOME = "${androidSdk}/libexec/android-sdk";
            JAVA_HOME = "${pkgs.jdk17_headless}";
            # Every CMake build compiles through ccache, so godot-cpp is
            # compiled once per version and reused by every build folder and
            # worktree (paths under the base dir are hashed relative).
            CMAKE_C_COMPILER_LAUNCHER = "ccache";
            CMAKE_CXX_COMPILER_LAUNCHER = "ccache";
            CCACHE_BASEDIR = "/home/claude/work";
            # numpy and Pillow wheels need the C++ runtime and zlib; the `bpy`
            # wheel (Blender as a Python module) links the X client libraries
            # even when it runs headless, so asset generation fails without
            # them on the path.
            LD_LIBRARY_PATH = pkgs.lib.makeLibraryPath [
              pkgs.stdenv.cc.cc.lib
              pkgs.zlib
              pkgs.xorg.libX11
              pkgs.xorg.libXrender
              pkgs.xorg.libXfixes
              pkgs.xorg.libXi
              pkgs.xorg.libXext
              pkgs.xorg.libXxf86vm
              pkgs.xorg.libXrandr
              pkgs.xorg.libXinerama
              pkgs.xorg.libXcursor
              pkgs.xorg.libxcb
              pkgs.xorg.libSM
              pkgs.xorg.libICE
              pkgs.libxkbcommon
              pkgs.libglvnd
            ];
            nativeBuildInputs = with pkgs; [
              godot_4_7
              blender
              clang
              scons
              cmake
              ninja
              ccache
              python3
              # The asset tools' Python packages (numpy, Pillow, bpy) come from the
              # Pipfile, which `.envrc` enters with `layout pipenv`. The wheels need
              # the native libraries above; pipenv provides the packages and the flake
              # provides what they link against.
              pipenv
              git
              gamescope
              # The broker behind the asset pipeline's job queue. A capture is one
              # Godot on one GPU and a build is one Blender; which of them may run at
              # once, in what order, and what happens when one finishes is a
              # scheduling problem that already has an answer, and the answer in this
              # language is Celery. Celery and its client come from the Pipfile like
              # every other Python package; this is the server they talk to, and it
              # is spoken to over a unix socket in .scratch/, never a port.
              redis
              gh
              wl-clipboard
              which
              nodejs
              chromium
              androidSdk
              jdk17_headless
              nixpkgs-emscripten.legacyPackages.${system}.emscripten
            ];
          };
        }
      );
    };
}
