#!/usr/bin/env python3
"""
Script to generate processor-specific event provider implementation.
This script generates a C++ file with hardware-specific performance counter events.
"""

import argparse
import os
import sys
from pathlib import Path
import platform
import csv
import re

def get_cpu_info():
    """Get CPU vendor, family, and model information."""
    try:
        # Try to read from /proc/cpuinfo on Linux
        if os.path.exists('/proc/cpuinfo'):
            with open('/proc/cpuinfo', 'r') as f:
                cpuinfo = f.read()

            vendor_id = None
            cpu_family = None
            model = None
            stepping = None

            for line in cpuinfo.split('\n'):
                if line.startswith('vendor_id'):
                    vendor_id = line.split(':')[1].strip()
                elif line.startswith('cpu family'):
                    cpu_family = int(line.split(':')[1].strip())
                elif line.startswith('model') and not line.startswith('model name'):
                    model = int(line.split(':')[1].strip())
                elif line.startswith('stepping'):
                    stepping = int(line.split(':')[1].strip())

                # Break after finding all info for the first CPU
                if vendor_id and cpu_family is not None and model is not None and stepping is not None:
                    break

            return vendor_id, cpu_family, model, stepping

        # Fallback for other systems - limited info available
        return None, None, None

    except Exception as e:
        print(f"[INCLUDE_PROCESSOR_EVENTS] Warning: Could not read CPU info: {e}", file=sys.stderr)
        return None, None, None

def get_architecture():
    if platform.machine().lower() in ['x86_64', 'amd64', 'i386', 'i686']:
        return 'x86'

    return None

def get_micro_architecture(architecture_dir):
    map_file_path = architecture_dir / 'cpu-to-micro-architecture-mapping.csv'
    if not map_file_path.is_file():
        return None

    vendor_id, cpu_family, model, stepping = get_cpu_info()
    if not vendor_id or cpu_family is None or model is None:
        return None

    # Format the CPU signature
    cpu_signature = f"{vendor_id}-{cpu_family}-{model:X}"
    cpu_signature_with_stepping = f"{cpu_signature}-{stepping}"

    with open(map_file_path, 'r') as f:
        reader = csv.DictReader(f)

        for row in reader:
            regex_pattern = f"^{row['CPU-Pattern']}$"

            if re.match(regex_pattern, cpu_signature, re.IGNORECASE) or re.match(regex_pattern, cpu_signature_with_stepping, re.IGNORECASE):
                return row['micro-architecture']

    return None

def read_events(events_file_path):
    events = []

    with open(events_file_path, 'r') as events_file:
        for line in events_file:
            parts = [p.strip() for p in line.split(',')]
            if len(parts) > 1:
                events.append(parts)

    return events

def generate_cpp_content(events):
    """Generate the C++ implementation content."""
    cpp_content = """#include <perfcpp/event_provider.h>
#include <perfcpp/counter_definition.h>
#include <linux/perf_event.h>

void
perf::ProcessorSpecificEventProvider::add_events(perf::CounterDefinition& counter_definition)
{"""
    for event in events:
        counter_config = f"CounterConfig{{PERF_TYPE_RAW, {event[1]}"
        if len(event) > 2:
            counter_config = f"{counter_config}, {event[2]}"
        counter_config = f"{counter_config}}}"

        cpp_content = cpp_content + f'\n  counter_definition.add("{event[0]}", {counter_config});'

    cpp_content = cpp_content + """
}
"""

    print(f"[INCLUDE_PROCESSOR_EVENTS] Generated source file with {len(events)} events.")
    return cpp_content


def write_output_file(output_path, content):
    """Write the generated content to the output file."""
    try:
        # Create directory if it doesn't exist
        output_dir = os.path.dirname(output_path)
        if output_dir and not os.path.exists(output_dir):
            os.makedirs(output_dir)

        # Write the file
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(content)

        return True

    except Exception as e:
        print(f"[INCLUDE_PROCESSOR_EVENTS] Error writing to {output_path}: {e}", file=sys.stderr)
        return False


def main():
    """Main function to handle command line arguments and generate the file."""
    parser = argparse.ArgumentParser(
        description="Generate processor-specific event provider implementation"
    )

    parser.add_argument(
        '--output', '-o',
        required=True,
        help='Output file path (e.g., src/processor_specific_event_provider.cpp)'
    )

    parser.add_argument(
        '--events',
        help='Events directory',
        default=None
    )

    parser.add_argument(
        '--verbose', '-v',
        action='store_true',
        help='Enable verbose output'
    )

    args = parser.parse_args()

    events_to_generate = []

    architecture = get_architecture()
    if architecture:
        architecture_path = Path(args.events) / architecture
        if architecture_path.is_dir():
            micro_architecture = get_micro_architecture(architecture_path)
            if micro_architecture:
                if args.verbose:
                    print(f"[INCLUDE_PROCESSOR_EVENTS] Detected micro-architecture: {micro_architecture}")
                micro_architecture_path = architecture_path / f'{micro_architecture}.csv'
                if micro_architecture_path.is_file():
                    events_to_generate = read_events(micro_architecture_path)


    # Generate the C++ content
    cpp_content = generate_cpp_content(events_to_generate)

    # Write to output file
    if write_output_file(args.output, cpp_content):
        if args.verbose:
            print(f"[INCLUDE_PROCESSOR_EVENTS] Wrote source file with processor-specific events: {args.output}")
        return 0
    else:
        print("[INCLUDE_PROCESSOR_EVENTS] Generation failed", file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main())