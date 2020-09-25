{
  pkgsUnstable ? import (fetchTarball "https://github.com/NixOS/nixpkgs-channels/archive/nixos-unstable.tar.gz") { }
}:
let
  pkgsMeson-0-49-2 = import (builtins.fetchTarball {
    url = "https://github.com/nixos/nixpkgs/archive/4599f2bb9a5a6b1482e72521ead95cb24e0aa819.tar.gz";
    sha256 = "04xr4xzcj64d5mf4jxzn5fsbz74rmf90qddp3jcdwj4skyik946d";
  }) { };
  demosSchedWithCC = cc:
    let
      pkgs = pkgsUnstable;
      myStdenv = with pkgs; (overrideCC stdenv (builtins.getAttr cc pkgs));
    in with pkgs;
    callPackage ./demos-sched.nix {
      stdenv = myStdenv;
      meson = meson.overrideAttrs (attrs: { depsHostHostPropagated = [ ]; });
      libyamlcpp = libyamlcpp.override { stdenv = myStdenv; };
    };
in {
  meson-0-49-2 = with pkgsMeson-0-49-2; callPackage ./demos-sched.nix { };
  unstable = with pkgsUnstable; callPackage ./demos-sched.nix { };
  gcc8 = demosSchedWithCC "gcc8";
  gcc7 = demosSchedWithCC "gcc7";
}
