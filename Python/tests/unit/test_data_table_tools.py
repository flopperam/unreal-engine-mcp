"""L1 Unit tests for data table tools.

Verifies payload serialization and input validation without a live Unreal Editor.
"""

import pytest
from unittest.mock import patch, MagicMock

import server.data_table_tools as data_table_tools


def _mock_ue_conn():
    mock = MagicMock()
    mock.send_command.return_value = {"success": True}
    return mock


class TestCreateDataTable:
    def test_sends_required_params(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = data_table_tools.create_data_table(
                table_path="/Game/Data/MyTable",
                row_struct_path="/Game/Structs/MyRow",
            )

        mock_ue.return_value.send_command.assert_called_once()
        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "create_data_table"
        payload = args[0][1]
        assert payload["table_path"] == "/Game/Data/MyTable"
        assert payload["row_struct_path"] == "/Game/Structs/MyRow"
        assert result["success"] is True

    def test_rejects_empty_table_path(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.create_data_table(table_path="", row_struct_path="/Game/Structs/MyRow")
        assert result.get("success") is False

    def test_rejects_empty_struct_path(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.create_data_table(table_path="/Game/Data/MyTable", row_struct_path="")
        assert result.get("success") is False

    def test_returns_error_when_no_connection(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=None):
            result = data_table_tools.create_data_table(
                table_path="/Game/Data/MyTable", row_struct_path="/Game/Structs/MyRow"
            )
        assert result.get("success") is False
        assert "connect" in result.get("error", "").lower()


class TestImportCsvToDataTable:
    def test_sends_required_params(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = data_table_tools.import_csv_to_data_table(
                table_path="/Game/Data/MyTable",
                csv_content="Name,Health,Speed\nRow1,100,5.0",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "import_csv_to_data_table"
        payload = args[0][1]
        assert payload["table_path"] == "/Game/Data/MyTable"
        assert payload["csv_content"] == "Name,Health,Speed\nRow1,100,5.0"
        assert result["success"] is True

    def test_rejects_empty_table_path(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.import_csv_to_data_table(table_path="", csv_content="a,b")
        assert result.get("success") is False

    def test_rejects_empty_csv_content(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.import_csv_to_data_table(table_path="/Game/Data/MyTable", csv_content="")
        assert result.get("success") is False


class TestAddDataTableRow:
    def test_sends_required_params(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = data_table_tools.add_data_table_row(
                table_path="/Game/Data/MyTable",
                row_name="Row1",
                row_data={"Health": 100, "Speed": 5.0},
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "add_data_table_row"
        payload = args[0][1]
        assert payload["table_path"] == "/Game/Data/MyTable"
        assert payload["row_name"] == "Row1"
        assert payload["row_data"] == {"Health": 100, "Speed": 5.0}
        assert result["success"] is True

    def test_rejects_empty_table_path(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.add_data_table_row(table_path="", row_name="Row1", row_data={})
        assert result.get("success") is False

    def test_rejects_empty_row_name(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.add_data_table_row(
                table_path="/Game/Data/MyTable", row_name="", row_data={}
            )
        assert result.get("success") is False


class TestDeleteDataTableRow:
    def test_sends_required_params(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = data_table_tools.delete_data_table_row(
                table_path="/Game/Data/MyTable",
                row_name="Row1",
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "delete_data_table_row"
        payload = args[0][1]
        assert payload["table_path"] == "/Game/Data/MyTable"
        assert payload["row_name"] == "Row1"
        assert result["success"] is True

    def test_rejects_empty_table_path(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.delete_data_table_row(table_path="", row_name="Row1")
        assert result.get("success") is False

    def test_rejects_empty_row_name(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.delete_data_table_row(
                table_path="/Game/Data/MyTable", row_name=""
            )
        assert result.get("success") is False


class TestUpdateDataTableRow:
    def test_sends_required_params(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = data_table_tools.update_data_table_row(
                table_path="/Game/Data/MyTable",
                row_name="Row1",
                row_data={"Health": 200, "Speed": 10.0},
            )

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "update_data_table_row"
        payload = args[0][1]
        assert payload["table_path"] == "/Game/Data/MyTable"
        assert payload["row_name"] == "Row1"
        assert payload["row_data"] == {"Health": 200, "Speed": 10.0}
        assert result["success"] is True

    def test_rejects_empty_table_path(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.update_data_table_row(table_path="", row_name="Row1", row_data={})
        assert result.get("success") is False

    def test_rejects_empty_row_name(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.update_data_table_row(
                table_path="/Game/Data/MyTable", row_name="", row_data={}
            )
        assert result.get("success") is False


class TestExportDataTableCSV:
    def test_sends_table_path(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = data_table_tools.export_data_table_csv(table_path="/Game/Data/MyTable")

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "export_data_table_csv"
        payload = args[0][1]
        assert payload["table_path"] == "/Game/Data/MyTable"
        assert result["success"] is True

    def test_rejects_empty_table_path(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.export_data_table_csv(table_path="")
        assert result.get("success") is False


class TestExportDataTableJSON:
    def test_sends_table_path(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()) as mock_ue:
            result = data_table_tools.export_data_table_json(table_path="/Game/Data/MyTable")

        args = mock_ue.return_value.send_command.call_args
        assert args[0][0] == "export_data_table_json"
        payload = args[0][1]
        assert payload["table_path"] == "/Game/Data/MyTable"
        assert result["success"] is True

    def test_rejects_empty_table_path(self):
        with patch("server.data_table_tools.get_unreal_connection", return_value=_mock_ue_conn()):
            result = data_table_tools.export_data_table_json(table_path="")
        assert result.get("success") is False
