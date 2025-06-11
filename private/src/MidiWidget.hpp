#pragma once
#include "plugin.hpp"

namespace SmartGrid
{
#ifndef IOS_BUILD
struct MidiDriverDisplayChoice : LedDisplayChoice
{
    struct MidiDriverItem : ui::MenuItem
    {
        midi::Port* m_port;
        int m_driverId;
        void onAction(const event::Action& e) override
        {
            m_port->setDriverId(m_driverId);
        }
    };

	midi::Port* m_port;
    
	void onAction(const event::Action& e) override
    {
		if (!m_port)
        {
			return;
        }
        
		createContextMenu();
	}

	virtual ui::Menu* createContextMenu()
    {
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel("MIDI driver"));
		for (int driverId : midi::getDriverIds())
        {
			MidiDriverItem* item = new MidiDriverItem;
			item->m_port = m_port;
			item->m_driverId = driverId;
			item->text = midi::getDriver(driverId)->getName();
			item->rightText = CHECKMARK(item->m_driverId == m_port->driverId);
			menu->addChild(item);
		}
        
		return menu;
	}

	void step() override
    {
		text = m_port ? m_port->getDriver()->getName() : "(No driver)";
	}
};

struct MidiDeviceDisplayChoice : LedDisplayChoice {

    struct MidiDeviceItem : ui::MenuItem
    {
        midi::Port* m_port;
        int m_deviceId;
        void onAction(const event::Action& e) override
        {
            m_port->setDeviceId(m_deviceId);
        }
    };
    
	midi::Port* m_port;
	void onAction(const event::Action& e) override
    {
		if (!m_port)
        {
			return;
        }
        
		createContextMenu();
	}

	virtual ui::Menu* createContextMenu()
    {
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel("MIDI device"));
        MidiDeviceItem* nullItem = new MidiDeviceItem;
        nullItem->m_port = m_port;
        nullItem->m_deviceId = -1;
        nullItem->text = "(No device)";
        nullItem->rightText = CHECKMARK(nullItem->m_deviceId == m_port->deviceId);
        menu->addChild(nullItem);
        
		for (int deviceId : m_port->getDeviceIds())
        {
			MidiDeviceItem* item = new MidiDeviceItem;
			item->m_port = m_port;
			item->m_deviceId = deviceId;
			item->text = m_port->getDeviceName(deviceId);
			item->rightText = CHECKMARK(item->m_deviceId == m_port->deviceId);
			menu->addChild(item);
		}
        
		return menu;
	}

	void step() override
    {
		text = m_port ? m_port->getDeviceName(m_port->deviceId) : "(No device)";
	}
};

struct MidiChannelDisplayChoice : LedDisplayChoice
{
    struct MidiChannelItem : ui::MenuItem
    {
        midi::Port* m_port;
        int m_channel;
        void onAction(const event::Action& e) override
        {
            m_port->channel = m_channel;
        }
    };

	midi::Port* m_port;
	void onAction(const event::Action& e) override
    {
		if (!m_port)
        {
			return;
        }
        
		createContextMenu();
	}

	virtual ui::Menu* createContextMenu()
    {
		ui::Menu* menu = createMenu();
		menu->addChild(createMenuLabel("MIDI channel"));
		for (int channel : m_port->getChannels())
        {
			MidiChannelItem* item = new MidiChannelItem;
			item->m_port = m_port;
			item->m_channel = channel;
			item->text = m_port->getChannelName(channel);
			item->rightText = CHECKMARK(item->m_channel == m_port->channel);
			menu->addChild(item);
		}
        
		return menu;
	}

	void step() override
    {
		text = m_port ? m_port->getChannelName(m_port->channel) : "Channel 1";
	}
};


struct MidiWidget : LedDisplay
{
	MidiDriverDisplayChoice* m_driverChoice;
	MidiDeviceDisplayChoice* m_deviceChoice;
	MidiChannelDisplayChoice* m_channelChoice;

	void SetMidiPort(midi::Port* port, bool includeChannel)
    {
		clearChildren();
		math::Vec pos;

        m_driverChoice = createWidget<MidiDriverDisplayChoice>(pos);
		m_driverChoice->box.size = Vec(box.size.x, 22.15f);
		m_driverChoice->color = nvgRGB(0xf0, 0xf0, 0xf0);
		m_driverChoice->m_port = port;
		addChild(m_driverChoice);
		pos = m_driverChoice->box.getBottomLeft();

		auto driverSeparator = createWidget<LedDisplaySeparator>(pos);
		driverSeparator->box.size.x = box.size.x;
		addChild(driverSeparator);

        m_deviceChoice = createWidget<MidiDeviceDisplayChoice>(pos);
		m_deviceChoice->box.size = Vec(box.size.x, 22.15f);
		m_deviceChoice->color = nvgRGB(0xf0, 0xf0, 0xf0);
		m_deviceChoice->m_port = port;
		addChild(m_deviceChoice);
		pos = m_deviceChoice->box.getBottomLeft();

        if (includeChannel)
        {
            auto deviceSeparator = createWidget<LedDisplaySeparator>(pos);
            deviceSeparator->box.size.x = box.size.x;
            addChild(deviceSeparator);

            m_channelChoice = createWidget<MidiChannelDisplayChoice>(pos);
            m_channelChoice->box.size = Vec(box.size.x, 22.15f);
            m_channelChoice->color = nvgRGB(0xf0, 0xf0, 0xf0);
            m_channelChoice->m_port = port;
            addChild(m_channelChoice);
        }
	}
};

struct ShapeDisplayChoice : LedDisplayChoice
{
    struct ShapeItem : ui::MenuItem
    {
        ShapeItem(ControllerShape* shape, ControllerShape myShape)
        {
            m_shape = shape;
            m_myShape = myShape;
        }

        ControllerShape* m_shape;
        ControllerShape m_myShape;

        void onAction(const event::Action& e) override
        {
            *m_shape = m_myShape;
        }
    };

    ControllerShape* m_shape;

    ShapeDisplayChoice(ControllerShape* shape)
    {
        m_shape = shape;
    }

    ShapeDisplayChoice()
    {
        m_shape = nullptr;
    }

    void onAction(const event::Action& e) override
    {
        if (!m_shape)
        {
            return;
        }

        createContextMenu();
    }

    virtual ui::Menu* createContextMenu()
    {
        ui::Menu* menu = createMenu();
        menu->addChild(createMenuLabel("Shape"));
        for (int i = 0; i < static_cast<int>(ControllerShape::NumShapes); i++)
        {
            ControllerShape myShape = static_cast<ControllerShape>(i);
            ShapeItem* item = new ShapeItem(m_shape, myShape);

            item->text = ControllerShapeToString(myShape);
            item->rightText = CHECKMARK(*m_shape == myShape);
            menu->addChild(item);
        }

        return menu;
    }

    void step() override
    {
        text = m_shape ? ControllerShapeToString(*m_shape) : "(No shape)";
    }
};

struct ShapeWidget : LedDisplay
{
    ShapeDisplayChoice* m_shapeChoice;

    void SetShape(ControllerShape* shape)
    {
        clearChildren();

        m_shapeChoice = createWidget<ShapeDisplayChoice>(Vec(0, 0));
        m_shapeChoice->box.size = Vec(box.size.x, 22.15f);
        m_shapeChoice->color = nvgRGB(0xf0, 0xf0, 0xf0);
        m_shapeChoice->m_shape = shape;
        addChild(m_shapeChoice);
    }
};
#endif
}
