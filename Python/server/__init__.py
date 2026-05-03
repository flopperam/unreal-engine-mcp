"""
Unreal Engine Advanced MCP Server

A streamlined MCP server focused on advanced composition tools for Unreal Engine.
Contains only the advanced tools from the expanded MCP tool system to keep tool count manageable.
"""

import logging
import os

from server.core import mcp, configure_logging, server_lifespan

# Re-export the mcp instance for any external consumers
__all__ = ["mcp", "bootstrap"]


def bootstrap():
    """Explicitly import tool modules to register their @mcp.tool() decorated tools.

    Call this before starting the MCP server. Keeping imports lazy avoids heavy
    side-effects during package import and prevents circular dependencies.
    """
    from server import actor_tools        # noqa: F401
    from server import material_tools      # noqa: F401
    from server import material_graph_tools  # noqa: F401      # noqa: F401
    from server import blueprint_tools    # noqa: F401
    from server import blueprint_graph_tools  # noqa: F401
    from server import world_building_tools  # noqa: F401


if __name__ == "__main__":
    configure_logging()
    bootstrap()
    logger = logging.getLogger("UnrealMCP_Advanced")
    logger.info("Starting Advanced MCP server with stdio transport")
    mcp.run(transport='stdio')
