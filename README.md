<picture>
  <source media="(prefers-color-scheme: dark)" srcset="https://github.com/hyblocker/OpenVR-SpaceCalibrator/blob/develop/.github/logo_light.png?raw=true">
  <source media="(prefers-color-scheme: light)" srcset="https://github.com/hyblocker/OpenVR-SpaceCalibrator/blob/develop/.github/logo_dark.png?raw=true">
  <img alt="Space Calibrator" src="https://github.com/hyblocker/OpenVR-SpaceCalibrator/blob/develop/.github/logo.png?raw=true">
</picture>

This program is designed to allow you to synchronise multiple playspaces with one another in SteamVR. This fork of Space Calibrator (spacecal) also supports [continuous calibration](#continuous-calibration).

Continuous calibration is a tracking mode which automatically aligns playspaces together, using a tracker on the headset.

This version of Space Calibrator 2.0 has been rewritten from the ground up for improved robustness, QOL improvements and less tracking issues, amongst others. For a list of differences compared with other versions, please see [the features list](#features)

## Installing

### Steam

> [!NOTE]  
> **Space Calibrator is also available on Steam.**

You may find [Space Calibrator on Steam here](https://s.team/a/3368750).

### From GitHub

To install Space Calibrator, please get the latest installer from the downloads page, and install it. Make sure that you have:
- Installed [Visual C++ Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).
- Installed SteamVR and run it at least once with a VR headset connected.
- SteamVR is not running before you run the installer. If SteamVR is running the installer will not be able to install Space Calibrator correctly.

## Calibration

If you do not wish to use continuous calibration, you will have to use regular calibration. This means that every so often you will have to sync your headset's playspace with your tracker's playspace.

To calibrate:
1. Copy the chaperone/guardian bounds from your HMD's play space
   > You will only have to do this once. Connect your VR headset and start SteamVR. Then go to space calibrator's window (it will be minimised), and click the "Copy Chaperone" button.

2. Open the SteamVR dashboard. At the bottom, click on the Space Calibrator icon.
3. In the Space Calibrator overlay, you'll see two lists at the top. On the left `Reference Space` column, select the controller you'll be calibrating along (e.g. Quest controller, Pico controller). On the right `Target Space`, select your SteamVR tracker (e.g. Vive Ultimate Tracker, Vive Tracker 3.0, Vive Ultimate Tracker). You can use the Identify button to make the controllers blink and tracker LEDs flash to see if you've selected the correct ones.
4. Click the "Start calibration" button, and start calibrating.

## Continuous Calibration

> [!IMPORTANT]  
> **A tracker attached on your headset is required for this.**

To enable continuous calibration mode, first select your headset on the left column, then the tracker on your headset on the right column. Once you've done so, click `Start Calibration`, and click cancel. Then click `Continuous Calibration` to enable continuous calibration.

1. Start SteamVR with the VR headset you wish to use.
2. Turn on **ONLY** the tracker which is attached on the VR headset.
3. Select the VR headset and tracker and calibrate.
4. Turn on your other devices.
5. You should see them line up with you as you after moving around your playspace for a bit for an initial calibration.

## Features

This version has been rewritten from scratch. It shares little code with the original repository but keeps similar ideas.

Major features:
- The UI has been reworked substantially to improve UX. The goal is to reduce the need of tutorials and have the app explain how to calibrate and what one may do to improve calibrations directly in app rather than elsewhere.
- Calibrations are now more streamlined. The calibration logic has been simplified to attempt minimising the chances of erroneous data being injected into a calibration sequence yielding poor calibrations. Furthermore, general overall improvements are present, such as improved tracker / controller prediction.
- Continuous calibration has been improved to reduce the frequency of mis-calibrations as much as possible.
- Relative calibrations. The aim is to re-formulate how a calibration is stored so that it is now relative to your headset, meaning that if your headset drifts your trackers would along with it, hiding the drift entirely.
- Robust logging. Logs are saved at `%APPDATA%/space-calibrator/logs` on Windows, and `~/.local/share/space-calibrator/logs` on Linux.
- Settings are saved to a JSON file at `%APPDATA%/space-calibrator/config.json` on Windows, and `~/.local/share/space-calibrator/config.json` on Linux.
- Linux support. The codebase can now be compiled for Linux x64 amd arm64. Official support assumes Steam Runtime 4, compatibility with other distributions is NOT guaranteed. This is not thoroughly tested but contributions / bug reports are appreciated.
- Space Calibration now supports translations. The app will default to showing text in your system language, and you may override it from the Settings page. For guidance regarding contributing translations please see [TRANSLATING.md](https://github.com/hyblocker/OpenVR-SpaceCalibrator/blob/nova/TRANSLATING.md)
- The UI renderer has been upgraded and now supports either OpenGL, DirectX11 (on Windows only) or Vulkan 1.3. The app will default to DirectX11 on Windows and Vulkan on Linux. This is to reduce issues for end users on buggy GPU rivers causing the app to fail to launch. You can override the renderer by passing `--renderer <opengl|dx11|vulkan>` as launch arguments.
- Base Station management has been integrated into the app to allow you to control them without 3rd party software.

## Help

If you need help with setting up this program, please check the [wiki](https://github.com/pushrax/OpenVR-SpaceCalibrator/wiki), or join the [Discord server](https://discord.gg/ja3WgNjC3z).
