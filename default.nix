{ crossSystem ? null
, withSubmodules ? false
, pkgs ? import <nixpkgs> { crossSystem = crossSystem; } }:
with pkgs;
callPackage ./demos-sched.nix { withSubmodules = withSubmodules; }
