#include <QtTest/QtTest>
#include <QSignalSpy>
#include <QCoreApplication>

#include "SlotMachine.h"
#include "Tower.h"
#include "HardwarePanelBackendFactory.h"
#include "HardwarePanelBackend.h"
#include "StubSimulatedHardwarePanelBackend.h"

class SlotMachineBackendTests : public QObject {
    Q_OBJECT

private slots:
    void slotMachineDoesNotRequireHardwareDependency();
    void slotMachineEmitsTowerHardwareIntentOnReset();
    void slotMachineApplyReelProbabilitiesFailsWithoutReel();
    void backendFactorySelectsStubSimulator();
    void stubSimulatorSignalsReadyAndRawResponses();
};

void SlotMachineBackendTests::slotMachineDoesNotRequireHardwareDependency() {
    SlotMachine machine;
    machine.setBalance(123.45);
    machine.setBet(2.30);

    QCOMPARE(machine.balance(), 123.45);
    QCOMPARE(machine.bet(), 2.30);
}

void SlotMachineBackendTests::slotMachineEmitsTowerHardwareIntentOnReset() {
    SlotMachine machine;
    QSignalSpy towerSpy(&machine, &SlotMachine::towerLevelChangedForHardware);
    QVERIFY(towerSpy.isValid());

    const auto towers = machine.findChildren<Tower *>();
    QVERIFY(!towers.isEmpty());
    QVERIFY(towers.first()->increase());

    machine.resetAllTowers();

    QVERIFY(towerSpy.count() >= 3);
    const auto firstArgs = towerSpy.takeFirst();
    QVERIFY(firstArgs.size() == 2);
    QCOMPARE(firstArgs.at(1).toInt(), 0);
}

void SlotMachineBackendTests::slotMachineApplyReelProbabilitiesFailsWithoutReel() {
    SlotMachine machine;
    QVariantMap probabilities;
    probabilities.insert("coin", 10);
    QVERIFY(!machine.applyReelProbabilities(probabilities));
}

void SlotMachineBackendTests::backendFactorySelectsStubSimulator() {
    qputenv("ALLESSPITZE_PANEL_BACKEND", "sim");
    auto backend = createHardwarePanelBackend();
    QVERIFY(backend != nullptr);
    QVERIFY(dynamic_cast<StubSimulatedHardwarePanelBackend *>(backend.get()) != nullptr);
    qunsetenv("ALLESSPITZE_PANEL_BACKEND");
}

void SlotMachineBackendTests::stubSimulatorSignalsReadyAndRawResponses() {
    StubSimulatedHardwarePanelBackend backend;
    QSignalSpy openedSpy(&backend, &HardwarePanelBackend::panelOpened);
    QSignalSpy readySpy(&backend, &HardwarePanelBackend::backendReady);
    QSignalSpy rawSpy(&backend, &HardwarePanelBackend::rawCommandResponse);

    backend.initializeBackend();
    backend.openPanel();

    QTRY_COMPARE(openedSpy.count(), 1);
    QTRY_COMPARE(readySpy.count(), 1);

    QVariantList data{1, 2, 3};
    backend.sendRawPanelCommand(0x11, data);
    QCOMPARE(rawSpy.count(), 1);

    const auto args = rawSpy.takeFirst();
    QCOMPARE(args.at(0).toInt(), 0x11);
    QCOMPARE(args.at(1).toBool(), true);
    QVERIFY(args.at(2).toByteArray().size() >= 3);
}

QTEST_GUILESS_MAIN(SlotMachineBackendTests)
#include "test_slotmachine_backend.moc"
