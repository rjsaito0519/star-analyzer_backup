#!/bin/tcsh
# setup_antigravity_skills.csh - Set up symlink for Antigravity (Gemini) custom skills in tcsh.

# Get scripts directory and repository root
set script_path=`lsof -p $$ | grep -o '/.*setup_antigravity_skills.csh' | head -n 1`
if ( "$script_path" == "" ) then
    # Fallback if lsof fails (usually run as ./script/setup_antigravity_skills.csh)
    set script_path=`pwd`/script/setup_antigravity_skills.csh
endif

set SCRIPT_DIR=`dirname "$script_path"`
set REPO_ROOT=`cd "$SCRIPT_DIR/.." && pwd`
set TARGET_LINK="$HOME/.gemini/config/plugins/star-analyzer"
set PLUGIN_DIR="$REPO_ROOT/.cursor"

echo "=== Antigravity (Gemini) Skills Setup (tcsh/csh) ==="
echo "Repository root: $REPO_ROOT"
echo "Plugin directory: $PLUGIN_DIR"
echo "Target symlink:  $TARGET_LINK"
echo ""

# Ensure ~/.gemini/config/plugins exists
mkdir -p "$HOME/.gemini/config/plugins"

# Handle existing files/links at target path
if ( -e "$TARGET_LINK" || -l "$TARGET_LINK" ) then
    echo "Warning: Target path '$TARGET_LINK' already exists."
    
    # Check if it is a symlink pointing to the correct place
    # readlink behaves differently on platforms, but we can do a quick check via ls -l or check if it links to PLUGIN_DIR
    set link_target=`readlink "$TARGET_LINK"`
    if ( "$link_target" == "$PLUGIN_DIR" ) then
        echo "The correct symlink is already configured. Nothing to do."
        exit 0
    endif

    # Otherwise backup/remove
    set backup_suffix=`date +%Y%m%d_%H%M%S`
    set BACKUP_PATH="${TARGET_LINK}.backup.${backup_suffix}"
    echo "Backing up existing target to '$BACKUP_PATH'..."
    mv "$TARGET_LINK" "$BACKUP_PATH"
endif

# Create symlink
echo "Creating symlink..."
ln -s "$PLUGIN_DIR" "$TARGET_LINK"

echo ""
echo "Success! The plugin has been registered."
echo "Please restart or reload your Antigravity (Gemini) IDE session to apply changes."
