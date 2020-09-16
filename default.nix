{ crossSystem ? null
, pkgs ? import <nixpkgs> { crossSystem = crossSystem; } }:
with pkgs;
callPackage ./demos-sched.nix { }
