# NixOS configuration for demos testing with hybrid cgroup hierarchy
#
# The easiest way to use this is to install nix and run nixos-shell on
# this file, e.g.:
#
#     nix-shell -p nixos-shell --command "nixos-shell vm-cgroup-v1.nix"
#
{ pkgs, ... }: {
  # Use the same config as vm.nix, but disable unified cgroup hierarchy.
  imports = [ ./vm.nix ];
  systemd.enableUnifiedCgroupHierarchy = false;
}
