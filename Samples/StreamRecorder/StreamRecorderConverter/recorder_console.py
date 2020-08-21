"""
 Copyright (c) Microsoft. All rights reserved.
 This code is licensed under the MIT License (MIT).
 THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
 ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
 IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
 PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
"""
import cmd
import json
import tarfile
import argparse
import urllib.request
from pathlib import Path
from urllib.parse import quote
from process_all import process_all


class RecorderShell(cmd.Cmd):
    dev_portal_browser = None
    w_path = None

    # cmd variables
    intro = 'Welcome to the recorder shell.   Type help or ? to list commands.\n'
    prompt = '(recorder console) '

    ruler = '-'

    def __init__(self, w_path, dev_portal_browser):
        super().__init__()
        self.dev_portal_browser = dev_portal_browser
        self.w_path = w_path

    def do_help(self, arg):
        print_help()

    def do_exit(self, arg):
        return True

    def do_list(self, arg):
        print("Device recordings:")
        self.dev_portal_browser.list_recordings()
        print("Workspace recordings:")
        list_workspace_recordings(self.w_path)

    def do_list_device(self, arg):
        self.dev_portal_browser.list_recordings()

    def do_list_workspace(self, arg):
        list_workspace_recordings(self.w_path)

    def do_download(self, arg):
        try:
            recording_idx = int(arg)
            if recording_idx is not None:
                self.dev_portal_browser.download_recording(
                    recording_idx, self.w_path)
        except ValueError:
            print(f"I can't download {arg}")


    def do_download_all(self, arg):
        for recording_idx in range(len(self.dev_portal_browser.recording_names)):
            self.dev_portal_browser.download_recording(recording_idx, self.w_path)

    def do_delete_all(self, arg):
        for _ in range(len(self.dev_portal_browser.recording_names)):
            self.dev_portal_browser.delete_recording(0)

    def do_delete(self, arg):
        try:
            recording_idx = int(arg)
            if recording_idx is not None:
                self.dev_portal_browser.delete_recording(recording_idx)
        except ValueError:
            print(f"I can't delete {arg}")

    def do_process(self, arg):
        try:
            recording_idx = int(arg)
            if recording_idx is not None:
                try:
                    recording_names = sorted(self.w_path.glob("*"))
                    recording_name = recording_names[recording_idx]
                except IndexError:
                    print("=> Recording does not exist")
                else:
                    process_all(
                        recording_name)
        except ValueError:
            print(f"I can't extract {arg}")


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--dev_portal_address", default="127.0.0.1:10080",
                        help="The IP address for the HoloLens Device Portal")
    parser.add_argument("--dev_portal_username", required=True,
                        help="The username for the HoloLens Device Portal")
    parser.add_argument("--dev_portal_password", required=True,
                        help="The password for the HoloLens Device Portal")
    parser.add_argument("--workspace_path", required=True,
                        help="Path to workspace folder used for downloading "
                             "recordings")

    args = parser.parse_args()

    return args


class DevicePortalBrowser(object):

    def connect(self, address, username, password):
        print("Connecting to HoloLens Device Portal...")
        self.url = "http://{}".format(address)
        password_manager = urllib.request.HTTPPasswordMgrWithDefaultRealm()
        password_manager.add_password(None, self.url, username, password)
        handler = urllib.request.HTTPBasicAuthHandler(password_manager)
        opener = urllib.request.build_opener(handler)
        opener.open(self.url)
        urllib.request.install_opener(opener)

        print("=> Connected to HoloLens at address:", self.url)

        print("Searching for StreamRecorder application...")

        response = urllib.request.urlopen(
            "{}/api/app/packagemanager/packages".format(self.url))
        packages = json.loads(response.read().decode())

        self.package_full_name = None
        for package in packages["InstalledPackages"]:
            if package["Name"] == "StreamRecorder":
                self.package_full_name = package["PackageFullName"]
                break
        assert self.package_full_name is not None, \
            "CV: Recorder package must be installed on HoloLens"

        print("=> Found StreamRecorder application with name:",
              self.package_full_name)

        print("Searching for recordings...")
        urlrequest = f'{self.url}/api/filesystem/apps/files?knownfolderid=LocalAppData&packagefullname={quote(self.package_full_name)}&path=\\LocalState'

        response = urllib.request.urlopen(urlrequest)
        recordings = json.loads(response.read().decode())

        self.recording_names = []
        for recording in recordings["Items"]:
            # Check if the recording contains any file data.
            request_url = "{}/api/filesystem/apps/files?knownfolderid=LocalAppData&packagefullname={}&path={}".format(
                self.url, self.package_full_name, "\\LocalState\\" + recording["Id"])
            response = urllib.request.urlopen(request_url)
            files = json.loads(response.read().decode())
            if len(files["Items"]) > 0:
                self.recording_names.append(recording["Id"])
        self.recording_names.sort()

        print("=> Found a total of {} recordings".format(
              len(self.recording_names)))

    def list_recordings(self, verbose=True):
        for i, recording_name in enumerate(self.recording_names):
            print("[{: 6d}]  {}".format(i, recording_name))

        if len(self.recording_names) == 0:
            print("=> No recordings found on device")

    def get_recording_name(self, recording_idx):
        try:
            return self.recording_names[recording_idx]
        except IndexError:
            print("=> Recording does not exist")

    def download_recording(self, recording_idx, w_path):
        recording_name = self.get_recording_name(recording_idx)
        if recording_name is None:
            return

        recording_path = w_path / recording_name
        recording_path.mkdir(exist_ok=True)

        print("Downloading recording {}...".format(recording_name))

        response = urllib.request.urlopen(
            "{}/api/filesystem/apps/files?knownfolderid="
            "LocalAppData&packagefullname={}&path=\\LocalState\\{}".format(
                self.url, self.package_full_name, recording_name))
        files = json.loads(response.read().decode())

        for file in files["Items"]:
            if file["Type"] != 32:
                continue

            destination_path = recording_path / file["Id"]
            if destination_path.exists():
                print("=> Skipping, already downloaded:", file["Id"])
                continue

            print("=> Downloading:", file["Id"])
            urllib.request.urlretrieve(
                "{}/api/filesystem/apps/file?knownfolderid=LocalAppData&"
                "packagefullname={}&filename=\\LocalState\\{}\\{}".format(
                    self.url, self.package_full_name,
                    recording_name, quote(file["Id"])), str(destination_path))

    def delete_recording(self, recording_idx):
        recording_name = self.get_recording_name(recording_idx)
        if recording_name is None:
            return

        print("Deleting recording {}...".format(recording_name))

        response = urllib.request.urlopen(
            "{}/api/filesystem/apps/files?knownfolderid="
            "LocalAppData&packagefullname={}&path=\\\\LocalState\\{}".format(
                self.url, self.package_full_name, recording_name))
        files = json.loads(response.read().decode())

        for file in files["Items"]:
            if file["Type"] != 32:
                continue

            print("=> Deleting:", file["Id"])
            urllib.request.urlopen(urllib.request.Request(
                "{}/api/filesystem/apps/file?knownfolderid=LocalAppData&"
                "packagefullname={}&filename=\\\\LocalState\\{}\\{}".format(
                    self.url, self.package_full_name,
                    recording_name, quote(file["Id"])), method="DELETE"))

        self.recording_names.remove(recording_name)


def print_help():
    print("Available commands:")
    print("  help:                     Print this help message")
    print("  exit:                     Exit the console loop")
    print("  list:                     List all recordings")
    print("  list_device:              List all recordings on the HoloLens")
    print("  list_workspace:           List all recordings in the workspace")
    print("  download X:               Download recording X from the HoloLens")
    print("  download_all:             Download all recordings from the HoloLens")
    print("  delete X:                 Delete recording X from the HoloLens")
    print("  delete_all:               Delete all recordings from the HoloLens")
    print("  process X:                Process recording X ")


def list_workspace_recordings(w_path):
    recording_names = sorted(w_path.glob("*"))
    for i, recording_name in enumerate(recording_names):
        print("[{: 6d}]  {}".format(i, recording_name.name))
    if len(recording_names) == 0:
        print("=> No recordings found in workspace")


def main():
    args = parse_args()

    w_path = Path(args.workspace_path)
    w_path.mkdir(exist_ok=True)

    dev_portal_browser = DevicePortalBrowser()
    dev_portal_browser.connect(args.dev_portal_address,
                               args.dev_portal_username,
                               args.dev_portal_password)

    print()
    print_help()
    print()

    dev_portal_browser.list_recordings()

    rs = RecorderShell(w_path, dev_portal_browser)
    rs.cmdloop()


if __name__ == "__main__":
    main()
