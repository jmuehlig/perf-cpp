#!/usr/bin/env python3
"""
PMU Event Downloader and Converter

This script downloads Performance Monitoring Unit (PMU) event JSON files
from the Linux kernel repository and converts them to CSV format for easier processing.

The script performs two main tasks:
1. Downloads JSON event files for all (for the time being) x86 micro-architectures
2. Converts the JSON data to CSV files with event names and codes
"""

import json
import re
from pathlib import Path
from typing import List, Optional, Tuple

import shutil
import requests


class PMUEventDownloader:
    """Downloads PMU event files from the Linux kernel repository."""

    BASE_URL = "https://api.github.com/repos/torvalds/linux/contents/tools/perf/pmu-events/arch"

    def __init__(self, output_directory: str):
        self.output_dir = Path(output_directory)

    def download_all_events(self, architectures) -> bool:
        """Download PMU events for all supported architectures."""
        print("Starting PMU event download...")

        for architecture in architectures:
            if not self._download_architecture(architecture):
                return False

        print(f"\nDownload complete! Files saved to: {self.output_dir}")
        return True

    def _download_architecture(self, architecture: str) -> bool:
        """Download events for a specific architecture."""
        arch_url = f"{self.BASE_URL}/{architecture}"
        arch_dir = self.output_dir / architecture

        try:
            # Get list of micro-architectures
            print(f"Fetching micro-architectures for {architecture}...")
            microarchs = self._get_microarchitectures(arch_url)
            print(f"Found {len(microarchs)} micro-architectures")

            # Download files for each microarchitecture
            for microarch in microarchs:
                self._download_microarchitecture(microarch, arch_dir)

            # Download mapfile if exists
            print(f"Fetching map file for {architecture}...")
            map_file = self._get_mapfile(arch_url)
            if map_file:
                print(f"Found {map_file['name']}")
                self._download_file(map_file, arch_dir)
            else:
                print(f"Cannot find map file")

            return True

        except requests.RequestException as e:
            print(f"Error processing architecture {architecture}: {e}")
            return False

    def _get_microarchitectures(self, url: str) -> List[dict]:
        """Get list of micro-architecture directories from GitHub API."""
        response = requests.get(url)
        response.raise_for_status()

        items = response.json()
        return [item for item in items if item['type'] == 'dir']

    def _get_mapfile(self, url: str) -> str:
        """Get mapfile.csv from GitHub API."""
        response = requests.get(url)
        response.raise_for_status()

        items = response.json()
        for item in items:
            if item['type'] == "file" and item["name"] == "mapfile.csv":
                return item

        return None

    def _download_microarchitecture(self, microarch: dict, base_dir: Path) -> None:
        """Download all JSON files for a specific micro-architecture."""
        microarch_name = microarch['name']
        microarch_url = microarch['url']

        print(f"\nProcessing micro-architecture: {microarch_name}")

        try:
            # Get JSON files in this directory
            json_files = self._get_json_files(microarch_url)
            print(f"  Found {len(json_files)} JSON files")

            # Download each file
            for json_file in json_files:
                self._download_file(json_file, base_dir / microarch_name)

        except requests.RequestException as e:
            print(f"  Error processing {microarch_name}: {e}")

    def _get_json_files(self, url: str) -> List[dict]:
        """Get list of JSON files from a micro-architecture directory."""
        response = requests.get(url)
        response.raise_for_status()

        files = response.json()
        return [f for f in files if f['name'].endswith('.json')]

    def _download_file(self, file_info: dict, target_dir: Path) -> None:
        """Download a single file to the target directory."""
        filename = file_info['name']
        download_url = file_info['download_url']
        filepath = target_dir / filename

        try:
            response = requests.get(download_url)
            response.raise_for_status()

            # Create directory if needed
            filepath.parent.mkdir(parents=True, exist_ok=True)

            # Write file
            with open(filepath, 'wb') as f:
                f.write(response.content)

            print(f"Downloaded: {filepath}")

        except requests.RequestException as e:
            print(f"Error downloading {filename}: {e}")


class PMUEventConverter:
    """Converts PMU event JSON files to CSV format."""

    def __init__(self, json_directory: str, csv_directory: str):
        self.json_dir = Path(json_directory)
        self.csv_dir = Path(csv_directory)

    def convert_all_events(self) -> None:
        """Convert all JSON files to CSV format."""
        print("Converting JSON files to CSV...")

        for architecture_dir in self.json_dir.iterdir():
            if architecture_dir.is_dir():
                self._convert_architecture(architecture_dir)


        print("Conversion complete!")

    def _convert_architecture(self, arch_dir: Path) -> None:
        """Convert all micro-architecture files for one architecture."""
        for microarch_dir in arch_dir.iterdir():
            if microarch_dir.is_dir():
                self._convert_microarchitecture(microarch_dir, arch_dir.name)

        self._copy_map_file(arch_dir.name)

    def _convert_microarchitecture(self, microarch_dir: Path, arch_name: str) -> None:
        """Convert JSON files from one micro-architecture to CSV."""
        events = []

        # Process all JSON files in this micro-architecture
        for json_file in microarch_dir.glob("*.json"):
            events.extend(self._extract_events_from_file(json_file))

        # Write CSV file if we have events
        if events:
            self._write_csv_file(events, arch_name, microarch_dir.name)

    def _extract_events_from_file(self, json_file: Path) -> List[Tuple[str, str, Optional[str]]]:
        """Extract event data from a single JSON file."""
        events = []

        try:
            with open(json_file, 'r', encoding='utf-8') as f:
                data = json.load(f)

            for event in data:
                if "EventName" in event:
                    event_codes = self._generate_event_codes(event)
                    if event_codes[0] is not None:
                        events.append((
                            event['EventName'],
                            hex(event_codes[0]),
                            hex(event_codes[1]) if event_codes[1] is not None else None
                        ))

        except (json.JSONDecodeError, IOError) as e:
            print(f"Error reading {json_file}: {e}")

        return events

    def _generate_event_codes(self, event: dict) -> Tuple[Optional[int], Optional[int]]:
        """Generate event codes from event configuration."""
        try:
            if "EventCode" not in event:
                return (None, None)

            # Calculate config0 (main event code)
            umask = 0 if "UMask" not in event else int(event["UMask"], 0)
            config0 = (umask << 8) | int(event["EventCode"], 0)
            config1 = None

            # Extract config1 from Filter if present
            if "Filter" in event:
                config1 = self._extract_config1_from_filter(event["Filter"])

            return (config0, config1)

        except (ValueError, KeyError):
            return (None, None)

    def _extract_config1_from_filter(self, filter_string: str) -> Optional[int]:
        """Extract config1 value from filter string."""
        pattern = r'([^,=]+)=([^,]+)'
        matches = re.findall(pattern, filter_string)

        for key, value in matches:
            if key.strip() == "config1":
                try:
                    return int(value, 0)
                except ValueError:
                    continue

        return None

    def _write_csv_file(self, events: List[Tuple[str, str, Optional[str]]],
                        arch_name: str, microarch_name: str) -> None:
        """Write events to a CSV file."""
        csv_path = self.csv_dir / arch_name / f"{microarch_name}.csv"
        csv_path.parent.mkdir(parents=True, exist_ok=True)

        with open(csv_path, 'w', encoding='utf-8') as f:
            for event_name, config0, config1 in events:
                line = f"{event_name}, {config0}"
                if config1 is not None:
                    line += f", {config1}"
                f.write(line + "\n")

        print(f"Created CSV: {csv_path} ({len(events)} events)")

    def _copy_map_file(self, arch_name):
        map_file_source_path = self.json_dir / arch_name / "mapfile.csv"
        if map_file_source_path.is_file():
            map_file_target_path = self.csv_dir / arch_name / "mapfile.csv"
            shutil.copy(str(map_file_source_path), str(map_file_target_path))
            print(f"Created Mapfile: {map_file_target_path}")


def main():
    """Main function to orchestrate the download and conversion process."""
    json_directory = "event-specifications"
    csv_directory = "events"

    # Download JSON files if they don't exist
    if not Path(json_directory).exists():
        downloader = PMUEventDownloader(json_directory)
        if not downloader.download_all_events(["x86"]):
            print("Download failed!")
            return 1
    else:
        print(f"JSON directory '{json_directory}' already exists, skipping download.")

    # Convert JSON to CSV
    converter = PMUEventConverter(json_directory, csv_directory)
    converter.convert_all_events()

    print("Process completed successfully!")
    return 0


if __name__ == "__main__":
    exit(main())