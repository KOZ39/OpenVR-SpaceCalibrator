---
name: Bug report
description: Report an issue with Space Calibrator.
title: '[Bug]: '
labels:
  - bug
body:
  - type: textarea
    attributes:
      label: Description
      description: A clear and short description of the problem
    validations:
      required: true
  - type: textarea
    attributes:
      label: Steps to reproduce
      description: >-
        Provide clear steps to trigger the issue if possible. If you're unsure,
        describe what you did prior to it happening and what you tried doing to
        solve it.
  - type: textarea
    attributes:
      label: Expected behaviour
      description: Describe what you expected to happen
  - type: input
    attributes:
      label: VR headset
      description: >-
        The VR headset you are using, e.g. Meta Quest 3, Pico 4, Steam Frame,
        Samsung Galaxy XR, etc.
      placeholder: Meta Quest 3
  - type: input
    attributes:
      label: Connection method
      description: >-
        How you are connecting your VR headset to your computer, eg Virtual
        Desktop, Steam Link, Air Link, Oculus Link Cable.
      placeholder: Steam Link
  - type: input
    attributes:
      label: What are you calibrating with?
      description: List the trackers you are using here if you're unsure.
      placeholder: Vive Tracker 3.0
  - type: markdown
    attributes:
      value: >-
        You are advised to also provide logs to help make it easier for me to
        troubleshoot your issues. You can go to Space Calibrator -> Settings and
        click the [View logs] button to open the folder. Please attach
        `log_overlay_latest.log` and `log_driver_latest.log`. Thank you!

---

<!--
If you are having trouble setting this up with SteamVR and would like help, the fastest way is to ask in the Discord group. https://discord.com/invite/m7g2Wyj

If you are having issues after updating SteamVR, please check for a new release at https://github.com/pushrax/OpenVR-SpaceCalibrator/releases before posting an issue.
-->
