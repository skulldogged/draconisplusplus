{
  nixpkgs,
  self,
  system,
  lib,
  pluginsSrc ? null,
  # devkitNix ? null,
  ...
}: let
  pkgs = import nixpkgs {
    inherit system;
    overlays = [self.overlays.default];
    # overlays = lib.optional (devkitNix != null) devkitNix.overlays.default;
  };

  dracPackages = import ./package.nix {inherit pkgs self lib pluginsSrc;};

  muslPackages =
    if pkgs.stdenv.isLinux
    then import ./musl.nix {inherit pkgs nixpkgs self lib pluginsSrc;}
    else {};
in
  dracPackages
  // muslPackages
  // {default = dracPackages."generic";}
