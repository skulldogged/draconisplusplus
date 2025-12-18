{
  description = "C/C++ environment";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable";
    treefmt-nix.url = "github:numtide/treefmt-nix";
    utils.url = "github:numtide/flake-utils";
  };

  outputs = {
    self,
    nixpkgs,
    treefmt-nix,
    utils,
    ...
  }: let
    inherit (nixpkgs) lib;

    # Optional override to provide a local/plugins source without requiring
    # an additional flake input. When unset, plugins are not vendored.
    pluginsSrc =
      let envPath = builtins.getEnv "DRACONIS_PLUGINS_SRC"; in
      if envPath == ""
      then null
      else builtins.path {path = envPath; name = "draconisplusplus-plugins";};
  in
    {homeModules.default = import ./nix/module.nix {inherit self;};}
    // utils.lib.eachDefaultSystem (
      system: let
        pkgs = import nixpkgs {
          inherit system;
        };

        llvmPackages = pkgs.llvmPackages_21;

        stdenv = with pkgs;
          (
            if hostPlatform.isLinux
            then stdenvAdapters.useMoldLinker
            else lib.id
          )
          llvmPackages.stdenv;

        devShellDeps = with pkgs;
          [
            (glaze.override {enableAvx2 = hostPlatform.isx86;})
          ]
          ++ (with pkgsStatic; [
            asio
            curl
            libunistring
            magic-enum
            sqlitecpp
            boost-ut
          ])
          ++ darwinPkgs
          ++ linuxPkgs;

        darwinPkgs = lib.optionals stdenv.isDarwin (with pkgs.pkgsStatic; [
          libiconv
          apple-sdk_15
        ]);

        linuxPkgs = lib.optionals stdenv.isLinux (with pkgs;
          [valgrind]
          ++ (with pkgsStatic; [
            dbus
            pugixml
            xorg.libxcb
            wayland
          ]));

        draconisPkgs = import ./nix {inherit nixpkgs self system lib pluginsSrc;};
      in {
        packages = draconisPkgs;
        checks = draconisPkgs;

        devShells.default = pkgs.mkShell.override {inherit stdenv;} {
          packages =
            (with pkgs; [
              alejandra
              bear
              cachix
              cmake
              hyperfine
              just
              llvmPackages.clang-tools
              meson
              mesonlsp
              ninja
              pkg-config

              (writeScriptBin "build" "meson compile -C build")
              (writeScriptBin "clean" ("meson setup build --wipe -Dprecompiled_config=true" + lib.optionalString pkgs.stdenv.isLinux " -Duse_linked_pci_ids=true"))
              (writeScriptBin "run" "meson compile -C build && build/draconis++")
            ])
            ++ devShellDeps;

          NIX_ENFORCE_NO_NATIVE = 0;



          shellHook =
            lib.optionalString pkgs.stdenv.hostPlatform.isDarwin ''
              export SDKROOT=${pkgs.pkgsStatic.apple-sdk_15}/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk
              export DEVELOPER_DIR=${pkgs.pkgsStatic.apple-sdk_15}
              export LDFLAGS="-L${pkgs.pkgsStatic.libiconvReal}/lib $LDFLAGS"
              export NIX_CFLAGS_COMPILE="-isysroot $SDKROOT"
              export NIX_CXXFLAGS_COMPILE="-isysroot $SDKROOT"
              export NIX_OBJCFLAGS_COMPILE="-isysroot $SDKROOT"
              export NIX_OBJCXXFLAGS_COMPILE="-isysroot $SDKROOT"
            ''
            + lib.optionalString pkgs.stdenv.hostPlatform.isLinux ''
              cp ${pkgs.pciutils}/share/pci.ids pci.ids
              chmod +w pci.ids
              objcopy -I binary -O default pci.ids pci_ids.o
              rm pci.ids
            '';
        };

        formatter = treefmt-nix.lib.mkWrapper pkgs {
          projectRootFile = "flake.nix";
          programs = {
            alejandra.enable = true;
            deadnix.enable = true;
            clang-format = {
              enable = true;
              package = pkgs.llvmPackages.clang-tools;
            };
          };
        };
      }
    );
}
