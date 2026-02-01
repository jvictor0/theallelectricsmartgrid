#!/usr/bin/env python3
"""
Sync patches and recordings between Mac and iPad via USB using pymobiledevice3.

Patches: Bi-directional sync (missing files copied both ways)
Recordings: One-way sync (iPad -> Mac, then deleted from iPad)

Requirements:
    pip install pymobiledevice3

Usage:
    python sync_ipad.py
"""

import os
import sys
from pathlib import Path
from typing import Optional

try:
    from pymobiledevice3.lockdown import create_using_usbmux
    from pymobiledevice3.services.house_arrest import HouseArrestService
    from pymobiledevice3.usbmux import list_devices
except ImportError:
    print("Error: pymobiledevice3 not installed.")
    print("Install with: pip install pymobiledevice3")
    sys.exit(1)


# App bundle identifier
#
APP_BUNDLE_ID = "com.theallelectricsmartgrid.smartgridone"

# Local paths on Mac
#
MAC_SMARTGRID_DIR = Path.home() / "Documents" / "SmartGridOne"
MAC_PATCHES_DIR = MAC_SMARTGRID_DIR / "patches"
MAC_RECORDINGS_DIR = MAC_SMARTGRID_DIR / "recordings"


def get_ipad_connection():
    """
    Connect to the first available iOS device via USB.
    """
    devices = list_devices()
    if not devices:
        print("No iOS devices found. Make sure your iPad is connected via USB.")
        return None
    
    device = devices[0]
    print(f"Found device: {device.serial}")
    
    try:
        lockdown = create_using_usbmux(serial=device.serial)
        return lockdown
    except Exception as e:
        print(f"Failed to connect to device: {e}")
        return None


def get_app_documents_path(afc: HouseArrestService) -> Optional[str]:
    """
    Find the SmartGridOne app's Documents directory on the iOS device.
    The AFC service for app containers uses relative paths from the app's container.
    """
    # When using HouseArrestService with the app's bundle ID, the root is the app's container
    # Documents folder is at /Documents relative to the container root
    #
    return "/Documents/SmartGridOne"


def ensure_dir_exists(afc: HouseArrestService, path: str):
    """
    Ensure a directory exists on the iOS device.
    """
    try:
        afc.makedirs(path)
    except Exception:
        # Directory might already exist
        #
        pass


def list_files_recursive(afc: HouseArrestService, path: str, base_path: str = "") -> list:
    """
    Recursively list all files in a directory on iOS device.
    Returns list of (relative_path, is_directory) tuples.
    """
    files = []
    try:
        entries = afc.listdir(path)
        for entry in entries:
            if entry in [".", ".."]:
                continue
            
            full_path = f"{path}/{entry}"
            rel_path = f"{base_path}/{entry}" if base_path else entry
            
            try:
                info = afc.stat(full_path)
                is_dir = info.get("st_ifmt") == "S_IFDIR"
                files.append((rel_path, is_dir, full_path))
                
                if is_dir:
                    files.extend(list_files_recursive(afc, full_path, rel_path))
            except Exception:
                pass
    except Exception as e:
        print(f"Error listing {path}: {e}")
    
    return files


def sync_patches_to_ipad(afc: HouseArrestService, ipad_patches_path: str):
    """
    Copy patches from Mac to iPad that don't exist on iPad.
    """
    if not MAC_PATCHES_DIR.exists():
        print(f"Mac patches directory does not exist: {MAC_PATCHES_DIR}")
        return
    
    ensure_dir_exists(afc, ipad_patches_path)
    
    # Get list of files on iPad
    #
    ipad_files = set()
    for rel_path, is_dir, _ in list_files_recursive(afc, ipad_patches_path):
        ipad_files.add(rel_path)
    
    # Walk through Mac patches and copy missing ones
    #
    for local_path in MAC_PATCHES_DIR.rglob("*"):
        rel_path = local_path.relative_to(MAC_PATCHES_DIR)
        rel_path_str = str(rel_path).replace("\\", "/")
        
        if rel_path_str not in ipad_files:
            ipad_path = f"{ipad_patches_path}/{rel_path_str}"
            
            if local_path.is_dir():
                print(f"Creating directory on iPad: {rel_path_str}")
                ensure_dir_exists(afc, ipad_path)
            else:
                print(f"Copying to iPad: {rel_path_str}")
                # Ensure parent directory exists
                #
                parent_path = f"{ipad_patches_path}/{rel_path.parent}".replace("\\", "/")
                if str(rel_path.parent) != ".":
                    ensure_dir_exists(afc, parent_path)
                
                # Read and upload file
                #
                with open(local_path, "rb") as f:
                    data = f.read()
                afc.set_file_contents(ipad_path, data)


def sync_patches_from_ipad(afc: HouseArrestService, ipad_patches_path: str):
    """
    Copy patches from iPad to Mac that don't exist on Mac.
    """
    MAC_PATCHES_DIR.mkdir(parents=True, exist_ok=True)
    
    # Get list of files on iPad
    #
    try:
        ipad_entries = list_files_recursive(afc, ipad_patches_path)
    except Exception as e:
        print(f"Could not list iPad patches: {e}")
        return
    
    for rel_path, is_dir, full_ipad_path in ipad_entries:
        local_path = MAC_PATCHES_DIR / rel_path
        
        if not local_path.exists():
            if is_dir:
                print(f"Creating directory on Mac: {rel_path}")
                local_path.mkdir(parents=True, exist_ok=True)
            else:
                print(f"Copying from iPad: {rel_path}")
                # Ensure parent directory exists
                #
                local_path.parent.mkdir(parents=True, exist_ok=True)
                
                # Download and write file
                #
                try:
                    data = afc.get_file_contents(full_ipad_path)
                    with open(local_path, "wb") as f:
                        f.write(data)
                except Exception as e:
                    print(f"  Error: {e}")


def sync_recordings_from_ipad(afc: HouseArrestService, ipad_recordings_path: str):
    """
    Copy recordings from iPad to Mac and delete from iPad.
    """
    MAC_RECORDINGS_DIR.mkdir(parents=True, exist_ok=True)
    
    # Get list of recordings on iPad
    #
    try:
        ipad_entries = list_files_recursive(afc, ipad_recordings_path)
    except Exception as e:
        print(f"Could not list iPad recordings: {e}")
        return
    
    # Filter to only files (not directories)
    #
    recording_files = [(rel, full) for rel, is_dir, full in ipad_entries if not is_dir]
    
    for rel_path, full_ipad_path in recording_files:
        local_path = MAC_RECORDINGS_DIR / rel_path
        
        print(f"Copying recording from iPad: {rel_path}")
        
        # Ensure parent directory exists
        #
        local_path.parent.mkdir(parents=True, exist_ok=True)
        
        # Download file
        #
        try:
            data = afc.get_file_contents(full_ipad_path)
            with open(local_path, "wb") as f:
                f.write(data)
            
            # Delete from iPad after successful copy
            #
            print(f"  Deleting from iPad: {rel_path}")
            afc.rm(full_ipad_path)
        except Exception as e:
            print(f"  Error: {e}")


def main():
    print("SmartGridOne iPad Sync")
    print("=" * 50)
    
    # Ensure local directories exist
    #
    MAC_SMARTGRID_DIR.mkdir(parents=True, exist_ok=True)
    MAC_PATCHES_DIR.mkdir(parents=True, exist_ok=True)
    MAC_RECORDINGS_DIR.mkdir(parents=True, exist_ok=True)
    
    # Connect to iPad
    #
    print("\nConnecting to iPad...")
    lockdown = get_ipad_connection()
    if not lockdown:
        return 1
    
    print(f"Connected to: {lockdown.display_name}")
    
    # Create AFC service for the app's container
    # In newer pymobiledevice3 versions, HouseArrestService directly provides AFC methods
    #
    try:
        print(f"\nAccessing app container: {APP_BUNDLE_ID}")
        afc = HouseArrestService(lockdown, bundle_id=APP_BUNDLE_ID)
    except Exception as e:
        print(f"Failed to access app container: {e}")
        print("Make sure SmartGridOne is installed on the iPad.")
        return 1
    
    # Get app documents path
    #
    app_docs = get_app_documents_path(afc)
    ipad_patches_path = f"{app_docs}/patches"
    ipad_recordings_path = f"{app_docs}/recordings"
    
    # Ensure directories exist on iPad
    #
    ensure_dir_exists(afc, app_docs)
    ensure_dir_exists(afc, ipad_patches_path)
    ensure_dir_exists(afc, ipad_recordings_path)
    
    # Sync patches (bi-directional)
    #
    print("\n--- Syncing Patches (Mac -> iPad) ---")
    sync_patches_to_ipad(afc, ipad_patches_path)
    
    print("\n--- Syncing Patches (iPad -> Mac) ---")
    sync_patches_from_ipad(afc, ipad_patches_path)
    
    # Sync recordings (one-way: iPad -> Mac, then delete from iPad)
    #
    print("\n--- Syncing Recordings (iPad -> Mac, delete from iPad) ---")
    sync_recordings_from_ipad(afc, ipad_recordings_path)
    
    print("\n" + "=" * 50)
    print("Sync complete!")
    
    return 0


if __name__ == "__main__":
    sys.exit(main())
