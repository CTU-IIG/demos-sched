# NixOS configuration for demos testing with unified cgroup hierarchy
#
# The easiest way to use this is to install nix and run nixos-shell on
# this file, e.g.:
#
#     nix-shell -p nixos-shell --command nixos-shell
#
{ pkgs, ... }: {
  nixpkgs.overlays = [ (self: super: {
    demos-sched = super.callPackage ../demos-sched.nix {};
  }) ];
  services.openssh.enable = true;
  environment.systemPackages = with pkgs; [
    demos-sched
  ];
}
