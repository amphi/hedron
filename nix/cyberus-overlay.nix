self: super:
let
  # We deliberately do not use the nixpkgs that we are overlaying to
  # build Hedron, because this means that the user does not get the
  # version we actually tested.
  thisPackage = import ./release.nix {};

  builds = thisPackage.hedron.builds;
in {
  hedron = super.pkgs.lib.warn
    "Using the hedron attributes directly is deprecated. Use hedron.default instead."
    {
      inherit (builds) default-release default-debug;

      builds = {
        inherit (builds) default-release default-debug;
      };
    };

  default = builds.default-release;
  debug = builds.default-debug;

  # As long as we do use our own nixpkgs to build all of the above,
  # there is no reason to expose the integration tests, as they
  # cannot show any different result than in our local pipeline.
}
