"""
Logging Configuration
=====================

Centralized logging setup for the application.
"""

import logging
import sys
from typing import Optional


def setup_logging(level: str = "INFO") -> None:
    """
    Configure application-wide logging.

    Args:
        level: Logging level (DEBUG, INFO, WARNING, ERROR, CRITICAL)
    """
    logging.basicConfig(
        level=getattr(logging, level.upper()),
        format="%(asctime)s - %(name)s - %(levelname)s - %(message)s",
        handlers=[
            logging.StreamHandler(sys.stdout)
        ]
    )

    # Reduce noise from third-party libraries
    logging.getLogger("urllib3").setLevel(logging.WARNING)
    logging.getLogger("httpx").setLevel(logging.WARNING)


def get_logger(name: Optional[str] = None) -> logging.Logger:
    """
    Get a logger instance.

    Args:
        name: Logger name (typically __name__ of the calling module)

    Returns:
        logging.Logger: Configured logger instance
    """
    return logging.getLogger(name or "heatpump")
