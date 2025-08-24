#!/usr/bin/env python3
"""
ESP-IDF OTA Extension

This module adds OTA deployment functionality to idf.py as a custom command.
"""

import os
import sys
from pathlib import Path

# Add the tools directory to Python path so we can import ota_deploy
tools_dir = Path(__file__).parent
sys.path.insert(0, str(tools_dir))

try:
    from ota_deploy import OTADeployer
except ImportError:
    print("Error: Could not import OTA deployment module")
    OTADeployer = None

def action_extensions(base_actions, project_path):
    """
    ESP-IDF extension function to add custom actions
    """
    
    def ota_flash(name, ctx, args):
        """Deploy firmware via OTA"""
        if OTADeployer is None:
            print("Error: OTA deployment not available")
            return 1
        
        # Change to project directory
        os.chdir(project_path)
        
        # Parse additional arguments
        force_update = getattr(args, 'force', False)
        check_only = getattr(args, 'check_only', False)
        config_file = getattr(args, 'config', 'ota_deploy_config.json')
        version = getattr(args, 'version', None)
        
        print("ESP32 BMS Monitor - OTA Deployment")
        print("=" * 40)
        
        try:
            deployer = OTADeployer(config_file)
            
            # Override version if specified
            if version:
                with open(deployer.config["build"]["version_file"], 'w') as f:
                    f.write(version)
            
            success = deployer.deploy(force_update, check_only)
            
            if success:
                print("\n✓ OTA deployment completed successfully!")
                return 0
            else:
                print("\n✗ OTA deployment failed!")
                return 1
                
        except Exception as e:
            print(f"\nError during OTA deployment: {e}")
            return 1
    
    def ota_check(name, ctx, args):
        """Check for OTA updates without deploying"""
        return ota_flash(name, ctx, type('Args', (), {
            'force': False,
            'check_only': True,
            'config': getattr(args, 'config', 'ota_deploy_config.json'),
            'version': getattr(args, 'version', None)
        })())
    
    def ota_config(name, ctx, args):
        """Configure OTA deployment settings"""
        config_file = getattr(args, 'config', 'ota_deploy_config.json')
        config_path = Path(config_file)
        
        if not config_path.exists():
            print(f"Creating default OTA configuration: {config_file}")
            # The OTADeployer constructor will create the default config
            OTADeployer(config_file)
            print(f"✓ Configuration file created: {config_file}")
            print("Please edit this file with your server and MQTT settings.")
        else:
            print(f"OTA configuration file exists: {config_file}")
            print("Edit this file to update your server and MQTT settings.")
        
        return 0
    
    # Add the custom actions
    ota_flash_action = {
        'names': ['ota_flash', 'ota-flash'],
        'callback': ota_flash,
        'short_help': 'Deploy firmware via OTA',
        'help': (
            'Build and deploy firmware using Over-The-Air updates. '
            'This command uploads the firmware to your OTA server and '
            'triggers the update via MQTT commands.'
        ),
    }
    
    ota_check_action = {
        'names': ['ota_check', 'ota-check'],
        'callback': ota_check,
        'short_help': 'Check for OTA updates',
        'help': 'Check for available OTA updates without deploying new firmware.',
    }
    
    ota_config_action = {
        'names': ['ota_config', 'ota-config'],
        'callback': ota_config,
        'short_help': 'Configure OTA deployment',
        'help': 'Create or display OTA deployment configuration.',
    }
    
    return [ota_flash_action, ota_check_action, ota_config_action]

def action_extensions_from_options(base_actions, project_path, project_options):
    """
    ESP-IDF extension function with access to project options
    """
    
    def add_ota_arguments(subparser):
        """Add OTA-specific arguments to the subparser"""
        subparser.add_argument(
            '--force',
            action='store_true',
            help='Force update (skip version check)'
        )
        subparser.add_argument(
            '--config',
            default='ota_deploy_config.json',
            help='OTA configuration file path'
        )
        subparser.add_argument(
            '--version',
            help='Override version string'
        )
        
    def add_ota_check_arguments(subparser):
        """Add OTA check-specific arguments"""
        subparser.add_argument(
            '--config',
            default='ota_deploy_config.json',
            help='OTA configuration file path'
        )
        
    def add_ota_config_arguments(subparser):
        """Add OTA config-specific arguments"""
        subparser.add_argument(
            '--config',
            default='ota_deploy_config.json',
            help='OTA configuration file path'
        )
    
    # Get the actions from the base function
    actions = action_extensions(base_actions, project_path)
    
    # Add argument parsers to the actions
    if len(actions) >= 1:
        actions[0]['arguments'] = add_ota_arguments
    if len(actions) >= 2:
        actions[1]['arguments'] = add_ota_check_arguments
    if len(actions) >= 3:
        actions[2]['arguments'] = add_ota_config_arguments
    
    return actions