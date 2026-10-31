function platform_pre_acpi(){
	iasl "Platform/Xiaomi/sm7150/AcpiTables/courbet/DSDT.dsl" || return "$?"
}
