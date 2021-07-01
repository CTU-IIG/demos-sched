# NixOS configuration for running demos tests in NixOS VM
#
# The easiest way to use this is to install nix and run nixos-shell on
# this file, e.g.:
#
#     nix-shell -p nixos-shell --command "nixos-shell vm-tests.nix"
#
{ pkgs, ... }: {
  imports = [ ./vm.nix ];
  boot.kernelParams = [ "systemd.unit=demos-sched-tests.service"];
  systemd.services = {
    demos-sched-tests = {
      description = "Run demos-sched test suite";
      requires = [ "multi-user.target" ];
      after = [ "multi-user.target" ];
      path = with pkgs; [ demos-sched perl bash gnumake diffutils ];
      script = ''
      prove --merge --directives --verbose *.t
    '';
      serviceConfig = {
        Type = "oneshot";
        WorkingDirectory = builtins.getEnv "PWD";
        StandardOutput = "journal+console";
        StandardError = "journal+console";
        Environment = ''TERM="${builtins.getEnv "TERM"}"'';
      };
      unitConfig = {
        SuccessAction = "poweroff-force";
        FailureAction = "poweroff-force";
      };
    };
  };
}
