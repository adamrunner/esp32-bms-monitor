#!/usr/bin/env python3
"""
ESP-IDF Extension for OTA Deployment

This file is automatically loaded by idf.py to provide custom commands.
It should be placed in the project root directory.
"""

import os
import sys
import subprocess
from pathlib import Path

def action_extensions(base_actions, project_path):
    """
    Add OTA deployment commands to idf.py
    ESP-IDF expects this function to return a dictionary with 'actions' and optional 'global_options'
    """
    
    def run_ota_deploy(args_list):
        """Run the OTA deployment script with given arguments"""
        tools_dir = Path(project_path) / "tools"
        ota_script = tools_dir / "ota_deploy.py"
        
        if not ota_script.exists():
            print(f"Error: OTA deployment script not found at {ota_script}")
            return 1
        
        cmd = [sys.executable, str(ota_script)] + args_list
        
        try:
            result = subprocess.run(cmd, cwd=project_path)
            return result.returncode
        except Exception as e:
            print(f"Error running OTA deployment: {e}")
            return 1
    
    def ota_flash_callback(name, ctx, global_args, **action_args):
        """Deploy firmware via OTA"""
        # Build arguments for ota_deploy.py
        deploy_args = []
        
        if action_args.get('force'):
            deploy_args.append('--force')
        
        if action_args.get('config'):
            deploy_args.extend(['--config', action_args['config']])
        
        if action_args.get('version'):
            deploy_args.extend(['--version', action_args['version']])
        
        return run_ota_deploy(deploy_args)
    
    def ota_check_callback(name, ctx, global_args, **action_args):
        """Check for OTA updates"""
        deploy_args = ['--check-only']
        
        if action_args.get('config'):
            deploy_args.extend(['--config', action_args['config']])
        
        return run_ota_deploy(deploy_args)
    
    def ota_config_callback(name, ctx, global_args, **action_args):
        """Configure OTA deployment settings"""
        config_file = action_args.get('config', 'ota_deploy_config.json')
        config_path = Path(project_path) / config_file
        
        if config_path.exists():
            print(f"OTA configuration file: {config_path}")
            print("Edit this file to configure your OTA server and MQTT settings.")
            
            # Show current config
            try:
                import json
                with open(config_path, 'r') as f:
                    config = json.load(f)
                print("\nCurrent configuration:")
                print(json.dumps(config, indent=2))
            except Exception as e:
                print(f"Error reading config: {e}")
        else:
            print(f"Creating default OTA configuration: {config_path}")
            # Run the deploy script to create default config
            return run_ota_deploy(['--config', str(config_path)])
        
        return 0
    
    # Define actions in the dictionary format ESP-IDF expects
    actions = {
        'ota_flash': {
            'callback': ota_flash_callback,
            'short_help': 'Deploy firmware via OTA',
            'help': (
                'Build and deploy firmware using Over-The-Air updates. '
                'This uploads firmware to the OTA server and triggers remote updates via MQTT.'
            ),
            'options': [
                {
                    'names': ['--force'],
                    'is_flag': True,
                    'help': 'Force update (skip version check)'
                },
                {
                    'names': ['--config'],
                    'help': 'OTA configuration file path',
                    'default': 'ota_deploy_config.json'
                },
                {
                    'names': ['--version'],
                    'help': 'Override version string'
                }
            ]
        },
        'ota_check': {
            'callback': ota_check_callback,
            'short_help': 'Check for OTA updates',
            'help': 'Check for available OTA updates without deploying new firmware.',
            'options': [
                {
                    'names': ['--config'],
                    'help': 'OTA configuration file path',
                    'default': 'ota_deploy_config.json'
                }
            ]
        },
        'ota_config': {
            'callback': ota_config_callback,
            'short_help': 'Configure OTA deployment',
            'help': 'Create or display OTA deployment configuration.',
            'options': [
                {
                    'names': ['--config'],
                    'help': 'OTA configuration file path',
                    'default': 'ota_deploy_config.json'
                }
            ]
        }
    }
    
    # Return in the format ESP-IDF expects
    return {
        'actions': actions,
        'global_options': []
    }

# Add argument definitions for the commands
def action_arguments():
    """Define command-line arguments for OTA commands"""
    return {
        'ota_flash': [
            {
                'names': ['--force'],
                'help': 'Force update (skip version check)',
                'action': 'store_true'
            },
            {
                'names': ['--config'],
                'help': 'OTA configuration file path',
                'default': 'ota_deploy_config.json'
            },
            {
                'names': ['--version'],
                'help': 'Override version string'
            }
        ],
        'ota_check': [
            {
                'names': ['--config'],
                'help': 'OTA configuration file path',
                'default': 'ota_deploy_config.json'
            }
        ],
        'ota_config': [
            {
                'names': ['--config'],
                'help': 'OTA configuration file path',
                'default': 'ota_deploy_config.json'
            }
        ]
    }

# Modern ESP-IDF extension interface
def action_extensions_with_args(base_actions, project_path, project_options):
    """
    Modern ESP-IDF extension interface that includes argument support
    """
    actions = action_extensions(base_actions, project_path)
    
    # Add arguments to actions
    arguments = action_arguments()
    
    for action in actions:
        action_name = action['names'][0]
        if action_name in arguments:
            action['arguments'] = []
            for arg in arguments[action_name]:
                action['arguments'].append({
                    'names': arg['names'],
                    'help': arg['help'],
                    'action': arg.get('action'),
                    'default': arg.get('default')
                })
    
    return actions