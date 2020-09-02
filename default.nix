{ pkgs ? import <nixpkgs> {} }:
with pkgs;
callPackage ./demos-sched.nix { }
